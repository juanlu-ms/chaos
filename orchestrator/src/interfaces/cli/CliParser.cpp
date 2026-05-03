#include <spdlog/spdlog.h>

#include <chrono>
#include <csignal>
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

#include "interfaces/tui/TuiApp.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "shared/StateBroadcaster.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::interfaces::cli {

namespace {

[[nodiscard]] bool isVerboseFlag(const std::string_view arg) { return arg == "--verbose" || arg == "-v"; }

[[nodiscard]] bool isHelpCommand(const std::string_view command) {
    return command == "help" || command == "--help" || command == "-h";
}

volatile std::sig_atomic_t g_interrupt_requested = 0;

using SignalHandler = void (*)(int);

// Restores the previous SIGINT handler on scope exit.
class SignalHandlerGuard {
public:
    explicit SignalHandlerGuard(SignalHandler previous) : previous_(previous) {}

    ~SignalHandlerGuard() {
        if (previous_ != SIG_ERR) {
            std::signal(SIGINT, previous_);
        }
    }

    SignalHandlerGuard(const SignalHandlerGuard&) = delete;
    SignalHandlerGuard& operator=(const SignalHandlerGuard&) = delete;

private:
    SignalHandler previous_;
};

// Signal handler to request interrupt-driven cancellation.
void signalHandler(int signal_number) {
    if (signal_number == SIGINT) {
        g_interrupt_requested = 1;
    }
}

}  // namespace

CliParser::CliParser(std::shared_ptr<containers::IContainerEngine> engine) : m_engine(std::move(engine)) {}

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

    if (command == "tui") {
        return handleTui();
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
        "  tui                   Launch interactive terminal UI\n"
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
        auto server = interfaces::web::Server(m_engine);
        server.listen(port);
    } catch (const std::system_error& ex) {
        SPDLOG_ERROR("Server error: {}", ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleRun(const std::string& manifestPath) const {
    try {
        auto manifest = manifests::ManifestParser::parseFromFile(manifestPath);
        SPDLOG_INFO("Executing manifest '{}' against target '{}'", manifest.test_name, manifest.target.id);

        // Build perturbations and schedule them asynchronously
        perturbations::PerturbationFactory factory;
        std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbation_instances;
        perturbation_instances.reserve(manifest.perturbations.size());
        for (const auto& spec : manifest.perturbations) {
            perturbation_instances.push_back(factory.create(m_engine, manifest.target, spec));
        }

        perturbations::PerturbationEngine pert_engine;
        const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));
        pert_engine.scheduleAllAsync(std::move(perturbation_instances), duration);

        observability::ObservabilityEngine obs(m_engine);
        shared::TargetState lastKnownState{};
        shared::StateBroadcaster broadcaster;

        // Register interrupt handler and reset interrupt state
        g_interrupt_requested = 0;
        const SignalHandler previous_handler = std::signal(SIGINT, signalHandler);
        const SignalHandlerGuard signal_guard(previous_handler);

        // Poll target state during the perturbation window.
        if (duration.count() == 0) {
            lastKnownState = obs.observe(manifest.target.id);
            broadcaster.broadcast(lastKnownState);
        } else {
            const auto end_time = std::chrono::steady_clock::now() + duration;
            while (std::chrono::steady_clock::now() < end_time && g_interrupt_requested == 0) {
                lastKnownState = obs.observe(manifest.target.id);
                broadcaster.broadcast(lastKnownState);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }

        // Cancel perturbations when interrupted
        if (g_interrupt_requested != 0) {
            SPDLOG_WARN("Interrupt received; canceling perturbations.");
            pert_engine.cancel();
        }

        // Wait for perturbations to teardown before validation
        pert_engine.waitForTeardown();

        // Evaluate manifest expectations against the last observed state
        if (!manifest.expectations.empty()) {
            SPDLOG_INFO("Evaluating {} expectation(s)...", manifest.expectations.size());
            const auto results = validation::ValidationEngine::validate(lastKnownState, manifest.expectations);

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

int CliParser::handleTui() const {
    auto app = interfaces::tui::TuiApp(m_engine);
    return app.run();
}

}  // namespace chaos::orchestrator::interfaces::cli
