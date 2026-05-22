#include "interfaces/cli/CliParser.hpp"

#include <spdlog/spdlog.h>
#include <unistd.h>

#include <charconv>
#include <chrono>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "interfaces/web/Server.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "shared/StateBroadcaster.hpp"
#include "signals/SignalHandlerGuard.hpp"

namespace chaos::orchestrator::interfaces::cli {

namespace {

[[nodiscard]] bool isVerboseFlag(const std::string_view arg) { return arg == "--verbose" || arg == "-v"; }

[[nodiscard]] bool isHelpCommand(const std::string_view command) {
    return command == "help" || command == "--help" || command == "-h";
}

}  // namespace

CliParser::CliParser(std::shared_ptr<containers::IContainerEngine> engine)
    : engine_(std::move(engine)), runner_(engine_) {}

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
                {
                    auto [ptr, ec] = std::from_chars(args[i + 1].data(), args[i + 1].data() + args[i + 1].size(), port);
                    if (ec != std::errc{}) {
                        SPDLOG_ERROR("Invalid port number: {}", args[i + 1]);
                        return 1;
                    }
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
        auto containers = engine_->listContainers();
        if (containers.empty()) {
            SPDLOG_INFO("No containers found.");
            return 0;
        }

        SPDLOG_INFO("{:<14} {:<30} {}", "CONTAINER ID", "NAME", "STATE");
        for (const auto& container : containers) {
            std::string displayId = container.id;
            if (displayId.empty()) {
                displayId = "<missing>";
            } else if (displayId.size() > 12) {
                displayId = displayId.substr(0, 12);
            }
            SPDLOG_INFO("{:<14} {:<30} {}", displayId, container.name, container.state);
        }
    } catch (const containers::ContainerEngineError& ex) {
        SPDLOG_ERROR("Failed to list containers: {}", ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleStop(const std::string& containerId) const {
    try {
        engine_->stopContainer(containerId);
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
        engine_->killContainer(containerId);
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
        auto server = interfaces::web::Server(engine_);
        server.listen(port);
    } catch (const std::system_error& ex) {
        SPDLOG_ERROR("Server error: {}", ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleRun(const std::string& manifestPath) const {
    try {
        auto manifest = runner_.parseManifest(manifestPath);
        SPDLOG_INFO("Executing manifest '{}' against target '{}'", manifest.test_name, manifest.target.id);

        auto perturbation_instances = runner_.buildPerturbations(manifest);
        const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));

        auto finalState = runPerturbationsLoop(std::move(perturbation_instances), manifest.target.id, duration);

        if (!runner_.validateExpectations(manifest, finalState)) {
            return 1;
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

shared::TargetState CliParser::runPerturbationsLoop(
    std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations, const std::string& targetId,
    std::chrono::seconds duration) const {
    perturbations::PerturbationEngine pert_engine;
    pert_engine.scheduleAllAsync(std::move(perturbations), duration);

    observability::ObservabilityEngine obs(engine_);
    shared::TargetState lastKnownState{};
    shared::StateBroadcaster broadcaster;

    signals::SignalHandlerGuard signal_guard;

    if (duration.count() == 0) {
        lastKnownState = obs.observe(targetId);
        broadcaster.broadcast(lastKnownState);
    } else {
        const auto end_time = std::chrono::steady_clock::now() + duration;

        while (std::chrono::steady_clock::now() < end_time) {
            int dummy = 0;
            ssize_t n = ::read(signal_guard.readEnd(), &dummy, sizeof(dummy));
            if (n > 0) {
                break;
            }

            lastKnownState = obs.observe(targetId);
            broadcaster.broadcast(lastKnownState);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    if (signal_guard.token().stop_requested()) {
        SPDLOG_WARN("Interrupt received; canceling perturbations.");
        pert_engine.cancel();
    }

    pert_engine.waitForTeardown();
    return lastKnownState;
}

}  // namespace chaos::orchestrator::interfaces::cli
