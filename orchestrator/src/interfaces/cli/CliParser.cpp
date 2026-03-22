#include <spdlog/spdlog.h>

#include <interfaces/cli/CliParser.hpp>
#include <interfaces/web/Server.hpp>
#include <string>
#include <utility>
#include <vector>

#include "manifests/ManifestParser.hpp"
#include "perturbations/PerturbationFactory.hpp"

namespace chaos::orchestrator::interfaces::cli {

CliParser::CliParser(std::shared_ptr<chaos::orchestrator::containers::IContainerEngine> engine)
    : m_engine(std::move(engine)) {}

int CliParser::run(int argc, char* argv[]) const {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::vector<std::string> args;
    args.emplace_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            spdlog::set_level(spdlog::level::debug);
        } else {
            args.push_back(arg);
        }
    }

    if (args.size() < 2) {
        printUsage();
        return 1;
    }

    const std::string& command = args[1];

    if (command == "help" || command == "--help" || command == "-h") {
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
                } catch (...) {
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
            const std::string displayId =
                c.id.empty() ? std::string("<missing>") : (c.id.size() > 12 ? c.id.substr(0, 12) : c.id);
            SPDLOG_INFO("{:<14} {:<30} {}", displayId, c.name, c.state);
        }
    } catch (const std::exception& ex) {
        SPDLOG_ERROR("Failed to list containers: {}", ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleStop(const std::string& containerId) const {
    try {
        m_engine->stopContainer(containerId);
        SPDLOG_INFO("Container {} stopped.", containerId);
    } catch (const std::exception& ex) {
        SPDLOG_ERROR("Failed to stop container {}: {}", containerId, ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleKill(const std::string& containerId) const {
    try {
        m_engine->killContainer(containerId);
        SPDLOG_INFO("Container {} killed.", containerId);
    } catch (const std::exception& ex) {
        SPDLOG_ERROR("Failed to kill container {}: {}", containerId, ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleServe(int port) const {
    try {
        auto server = chaos::orchestrator::interfaces::web::Server(m_engine);
        server.listen(port);
    } catch (const std::exception& ex) {
        SPDLOG_ERROR("Server error: {}", ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleRun(const std::string& manifestPath) const {
    try {
        auto manifest = chaos::orchestrator::manifests::ManifestParser::parse(manifestPath);
        SPDLOG_INFO("Executing manifest '{}' against target '{}'", manifest.test_name, manifest.target.name);

        chaos::orchestrator::perturbations::PerturbationFactory factory;
        for (const auto& pert_spec : manifest.perturbations) {
            SPDLOG_INFO("Applying perturbation '{}'", pert_spec.type);
            auto perturbation = factory.create(m_engine, pert_spec);
            perturbation->apply(manifest.target);
        }

        SPDLOG_INFO("All perturbations applied successfully.");
    } catch (const std::exception& ex) {
        SPDLOG_ERROR("Failed to run manifest: {}", ex.what());
        return 1;
    }
    return 0;
}

}  // namespace chaos::orchestrator::interfaces::cli
