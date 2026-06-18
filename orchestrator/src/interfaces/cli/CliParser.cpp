#include "interfaces/cli/CliParser.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "core/IRunObserver.hpp"
#include "core/ObservationLoop.hpp"
#include "core/RunOrchestrator.hpp"
#include "core/SharedState.hpp"
#include "core/StateBroadcaster.hpp"
#include "manifests/ManifestParser.hpp"
#include "signals/SignalHandlerGuard.hpp"

namespace chaos::orchestrator::interfaces::cli {

namespace {

[[nodiscard]] bool isVerboseFlag(const std::string_view arg) { return arg == "--verbose" || arg == "-v"; }

[[nodiscard]] bool isHelpCommand(const std::string_view command) {
    return command == "help" || command == "--help" || command == "-h";
}

class CliRunObserver final : public core::IRunObserver {
public:
    void onStateUpdate(const core::TargetState& state) override { broadcaster_.broadcast(state); }

    void onPhaseChange(std::string_view phase) override { SPDLOG_INFO("Entering phase: {}", phase); }

    void onNetworkLatencyUpdate(std::optional<double> /*latency*/) override {}

private:
    core::StateBroadcaster broadcaster_;
};

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
            fmt::print(stderr, "'stop' requires a container ID\n");
            return 1;
        }
        return handleStop(args[2]);
    }

    if (command == "kill") {
        if (args.size() < 3) {
            fmt::print(stderr, "'kill' requires a container ID\n");
            return 1;
        }
        return handleKill(args[2]);
    }

    if (command == "run") {
        if (args.size() < 3) {
            fmt::print(stderr, "'run' requires a path to a manifest JSON\n");
            return 1;
        }
        return handleRun(args[2]);
    }

    fmt::print(stderr, "Unknown command: '{}'\n", command);
    printUsage();
    return 1;
}

void CliParser::printUsage() const {
    fmt::print(stdout,
               "Usage: chaos <command> [options]\n"
               "\n"
               "Chaos - Resilience Tool for Docker Containers\n"
               "\n"
               "Commands:\n"
               "  list                  List all containers\n"
               "  stop  <container_id>  Stop a running container\n"
               "  kill  <container_id>  Kill a running container\n"
               "  run   <manifest.json> Execute a chaos manifest\n"
               "  help                  Show this help message\n"
               "\n"
               "Options:\n"
               "  -v, --verbose         Enable debug logging\n");
}

int CliParser::handleList() const {
    try {
        auto containers = engine_->listContainers();
        if (containers.empty()) {
            fmt::print(stdout, "No containers found.\n");
            return 0;
        }

        fmt::print(stdout, "{:<14} {:<30} {}\n", "CONTAINER ID", "NAME", "STATE");
        for (const auto& container : containers) {
            std::string displayId = container.id;
            if (displayId.empty()) {
                displayId = "<missing>";
            } else if (displayId.size() > 12) {
                displayId = displayId.substr(0, 12);
            }
            fmt::print(stdout, "{:<14} {:<30} {}\n", displayId, container.name, container.state);
        }
    } catch (const containers::ContainerEngineError& ex) {
        fmt::print(stderr, "Failed to list containers: {}\n", ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleStop(const std::string& containerId) const {
    try {
        engine_->stopContainer(containerId);
        fmt::print(stdout, "Container {} stopped.\n", containerId);
    } catch (const std::invalid_argument& ex) {
        fmt::print(stderr, "Failed to stop container {}: {}\n", containerId, ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        fmt::print(stderr, "Failed to stop container {}: {}\n", containerId, ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleKill(const std::string& containerId) const {
    try {
        engine_->killContainer(containerId);
        fmt::print(stdout, "Container {} killed.\n", containerId);
    } catch (const std::invalid_argument& ex) {
        fmt::print(stderr, "Failed to kill container {}: {}\n", containerId, ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        fmt::print(stderr, "Failed to kill container {}: {}\n", containerId, ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleRun(const std::string& manifestPath) const {
    try {
        auto manifest = runner_.parseManifest(manifestPath);
        SPDLOG_INFO("Executing manifest '{}' against target '{}'", manifest.test_name, manifest.target.id);

        auto perturbation_instances = runner_.buildPerturbations(manifest);

        core::SharedState state;
        CliRunObserver observer;

        core::ObservationLoop::Config loopConfig;
        loopConfig.metricsInterval = std::chrono::milliseconds(200);
        loopConfig.logsInterval = std::chrono::milliseconds(2000);
        loopConfig.continuousExpectations = manifest.expectations;

        signals::SignalHandlerGuard signal_guard;

        core::ObservationLoop obsLoop(engine_, manifest.target.id, state, observer, loopConfig);
        obsLoop.start(signal_guard.token());

        core::RunOrchestrator orchestrator(runner_);
        auto runResult =
            orchestrator.run(manifest, state, observer, std::move(perturbation_instances), signal_guard.token());

        printRunResults(runResult);
        if (!runResult.passed) {
            return 1;
        }
    } catch (const manifests::ManifestParserError& ex) {
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    } catch (const std::invalid_argument& ex) {
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    } catch (const std::system_error& ex) {
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    }
    return 0;
}

void CliParser::printRunResults(const core::RunResult& runResult) const {
    for (const auto& result : runResult.results) {
        if (result.passed) {
            fmt::print(stdout, "  ✓ {}: {}\n", result.expectationType, result.message);
        } else {
            fmt::print(stderr, "  ✗ {}: {}\n", result.expectationType, result.message);
        }
    }
}

}  // namespace chaos::orchestrator::interfaces::cli
