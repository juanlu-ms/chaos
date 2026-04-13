#include <spdlog/spdlog.h>

#include <chrono>
#include <interfaces/cli/CliParser.hpp>
#include <interfaces/web/Server.hpp>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::interfaces::cli {

namespace {

[[nodiscard]] bool isVerboseFlag(const std::string_view arg) { return arg == "--verbose" || arg == "-v"; }

[[nodiscard]] bool isHelpCommand(const std::string_view command) {
    return command == "help" || command == "--help" || command == "-h";
}

}  // namespace

CliParser::CliParser(std::shared_ptr<chaos::orchestrator::containers::IContainerEngine> engine)
    : m_engine(std::move(engine)) {}

int CliParser::run(std::span<char*> argv) const {
    if (argv.size() < 2) {
        printUsage();
        return 1;
    }

    std::vector<std::string> args;
    args.emplace_back(argv[0]);
    for (std::size_t i = 1; i < argv.size(); ++i) {
        std::string arg = argv[i];
        if (isVerboseFlag(arg)) {
            spdlog::set_level(spdlog::level::debug);
        } else {
            args.push_back(arg);
        }
    }

    if (args.size() < 2) {
        printUsage();
        return 1;
    }

    return dispatchCommand(args);
}

int CliParser::dispatchCommand(const std::vector<std::string>& args) const {
    const std::string& command = args[1];

    if (isHelpCommand(command)) {
        printUsage();
        return 0;
    }

    if (command == "list") {
        return handleList();
    }

    if (command == "stop") {
        if (args.size() < 3) {
            SPDLOG_ERROR("'stop' requires a container ID");
            return 1;
        }
        return handleStop(args[2]);
    }

    if (command == "kill") {
        if (args.size() < 3) {
            SPDLOG_ERROR("'kill' requires a container ID");
            return 1;
        }
        return handleKill(args[2]);
    }

    if (command == "run") {
        if (args.size() < 3) {
            SPDLOG_ERROR("'run' requires a path to a manifest JSON");
            return 1;
        }
        return handleRun(args[2]);
    }

    if (command == "serve") {
        int port = 8080;
        for (size_t i = 2; i + 1 < args.size(); ++i) {
            if (args[i] == "--port") {
                try {
                    port = std::stoi(args[i + 1]);
                } catch (const std::invalid_argument&) {
                    SPDLOG_ERROR("Invalid port number: {}", args[i + 1]);
                    return 1;
                } catch (const std::out_of_range&) {
                    SPDLOG_ERROR("Invalid port number: {}", args[i + 1]);
                    return 1;
                }
            }
        }
        return handleServe(port);
    }

    SPDLOG_ERROR("Unknown command: '{}'", command);
    printUsage();
    return 1;
}

void CliParser::printUsage() const {
    SPDLOG_INFO(
        "Usage: chaos <command> [options]\n"
        "\n"
        "Chaos - Resilience Tool for Docker Containers\n"
        "\n"
        "Commands:\n"
        "  list                  List all containers\n"
        "  stop  <container_id>  Stop a running container\n"
        "  kill  <container_id>  Kill a running container\n"
        "  run   <manifest.json> Execute a chaos manifest\n"
        "  serve [--port <n>]    Start the web server (default: 8080)\n"
        "  help                  Show this help message\n"
        "\n"
        "Options:\n"
        "  -v, --verbose         Enable debug logging\n");
}

int CliParser::handleList() const {
    try {
        auto containers = m_engine->listContainers();
        if (containers.empty()) {
            SPDLOG_INFO("No containers found.");
            return 0;
        }

        SPDLOG_INFO("{:<14} {:<30} {}", "CONTAINER ID", "NAME", "STATE");
        for (const auto& c : containers) {
            std::string displayId = c.id;
            if (displayId.empty()) {
                displayId = "<missing>";
            } else if (displayId.size() > 12) {
                displayId = displayId.substr(0, 12);
            }
            SPDLOG_INFO("{:<14} {:<30} {}", displayId, c.name, c.state);
        }
    } catch (const containers::ContainerEngineError& ex) {
        SPDLOG_ERROR("Failed to list containers: {}", ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleStop(const std::string& containerId) const {
    try {
        m_engine->stopContainer(containerId);
        SPDLOG_INFO("Container {} stopped.", containerId);
    } catch (const std::invalid_argument& ex) {
        SPDLOG_ERROR("Failed to stop container {}: {}", containerId, ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        SPDLOG_ERROR("Failed to stop container {}: {}", containerId, ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleKill(const std::string& containerId) const {
    try {
        m_engine->killContainer(containerId);
        SPDLOG_INFO("Container {} killed.", containerId);
    } catch (const std::invalid_argument& ex) {
        SPDLOG_ERROR("Failed to kill container {}: {}", containerId, ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        SPDLOG_ERROR("Failed to kill container {}: {}", containerId, ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleServe(int port) const {
    try {
        auto server = chaos::orchestrator::interfaces::web::Server(m_engine);
        server.listen(port);
    } catch (const std::system_error& ex) {
        SPDLOG_ERROR("Server error: {}", ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleRun(const std::string& manifestPath) const {
    try {
        auto manifest = chaos::orchestrator::manifests::ManifestParser::parseFromFile(manifestPath);
        SPDLOG_INFO("Executing manifest '{}' against target '{}'", manifest.test_name, manifest.target.id);

        chaos::orchestrator::perturbations::PerturbationFactory factory;
        std::vector<std::unique_ptr<chaos::orchestrator::perturbations::IPerturbation>> active_perturbations;

        // Structured cleanup guard to ensure perturbations are reverted
        struct RevertGuard {
            std::vector<std::unique_ptr<chaos::orchestrator::perturbations::IPerturbation>>& perts;
            ~RevertGuard() {
                for (auto it = perts.rbegin(); it != perts.rend(); ++it) {
                    try {
                        (*it)->revert();
                    } catch (const std::exception& e) {
                        SPDLOG_ERROR("Failed to revert perturbation: {}", e.what());
                    }
                }
            }
        } guard{active_perturbations};

        for (const auto& pert_spec : manifest.perturbations) {
            SPDLOG_INFO("Applying perturbation '{}'", pert_spec.type);
            auto perturbation = factory.create(m_engine, manifest.target, pert_spec);
            perturbation->apply();
            active_perturbations.push_back(std::move(perturbation));
        }

        SPDLOG_INFO("All perturbations applied successfully.");

        if (manifest.duration_s.has_value() && manifest.duration_s.value() > 0) {
            SPDLOG_INFO("Waiting {}s for faults to inject...", manifest.duration_s.value());
            std::this_thread::sleep_for(std::chrono::seconds(manifest.duration_s.value()));
        }

        // Evaluate manifest expectations
        if (!manifest.expectations.empty()) {
            SPDLOG_INFO("Evaluating {} expectation(s)...", manifest.expectations.size());
            chaos::orchestrator::observability::ObservabilityEngine obs(m_engine);
            chaos::orchestrator::validation::ValidationEngine validator(std::move(obs));
            const auto results = validator.validate(manifest.target.id, manifest.expectations);

            bool anyFailed = false;
            for (const auto& result : results) {
                if (!result.passed) {
                    anyFailed = true;
                }
            }

            if (anyFailed) {
                SPDLOG_ERROR("Chaos run FAILED: one or more expectations were not met.");
                return 1;
            }
            SPDLOG_INFO("Chaos run PASSED: all expectations met.");
        }
    } catch (const manifests::ManifestParserError& ex) {
        SPDLOG_ERROR("Failed to run manifest: {}", ex.what());
        return 1;
    } catch (const std::invalid_argument& ex) {
        SPDLOG_ERROR("Failed to run manifest: {}", ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        SPDLOG_ERROR("Failed to run manifest: {}", ex.what());
        return 1;
    } catch (const std::system_error& ex) {
        SPDLOG_ERROR("Failed to run manifest: {}", ex.what());
        return 1;
    }
    return 0;
}

}  // namespace chaos::orchestrator::interfaces::cli
