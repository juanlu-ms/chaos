#include "interfaces/cli/CliParser.hpp"

#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <memory>
#include <span>
#include <stop_token>
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
#include "perturbations/PerturbationFactory.hpp"
#include "shared/StateBroadcaster.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::interfaces::cli {

namespace {

[[nodiscard]] bool isVerboseFlag(const std::string_view arg) { return arg == "--verbose" || arg == "-v"; }

[[nodiscard]] bool isHelpCommand(const std::string_view command) {
    return command == "help" || command == "--help" || command == "-h";
}

std::stop_source g_stop_source;
int g_signal_pipe[2] = {-1, -1};

void signalHandler(int) {
    int dummy = 1;
    [[maybe_unused]] ssize_t result = ::write(g_signal_pipe[1], &dummy, sizeof(dummy));
}

class SignalHandlerGuard {
public:
    SignalHandlerGuard() {
        if (::pipe(g_signal_pipe) != 0) {
            g_signal_pipe[0] = g_signal_pipe[1] = -1;
            return;
        }
        if (::fcntl(g_signal_pipe[0], F_SETFL, O_NONBLOCK) == -1) {
            ::close(g_signal_pipe[0]);
            ::close(g_signal_pipe[1]);
            g_signal_pipe[0] = g_signal_pipe[1] = -1;
            return;
        }
        struct sigaction sa = {};
        sa.sa_handler = signalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        previous_valid_ = (::sigaction(SIGINT, &sa, &previous_) == 0);
    }

    ~SignalHandlerGuard() noexcept {
        if (previous_valid_) {
            ::sigaction(SIGINT, &previous_, nullptr);
        }
        if (g_signal_pipe[0] != -1) {
            ::close(g_signal_pipe[0]);
        }
        if (g_signal_pipe[1] != -1) {
            ::close(g_signal_pipe[1]);
        }
    }

    SignalHandlerGuard(const SignalHandlerGuard&) = delete;
    SignalHandlerGuard& operator=(const SignalHandlerGuard&) = delete;
    SignalHandlerGuard(SignalHandlerGuard&&) = delete;
    SignalHandlerGuard& operator=(SignalHandlerGuard&&) = delete;

    [[nodiscard]] int readEnd() const { return g_signal_pipe[0]; }
    [[nodiscard]] std::stop_token token() const { return g_stop_source.get_token(); }

private:
    struct sigaction previous_ = {};
    bool previous_valid_ = false;
};

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
        auto manifest = parseManifest(manifestPath);
        SPDLOG_INFO("Executing manifest '{}' against target '{}'", manifest.test_name, manifest.target.id);

        auto perturbation_instances = buildPerturbations(manifest);
        const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));

        auto finalState = runPerturbationsLoop(std::move(perturbation_instances), manifest.target.id, duration);

        if (!validateExpectations(manifest, finalState)) {
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

manifests::ChaosManifest CliParser::parseManifest(const std::string& path) const {
    return manifests::ManifestParser::parseFromFile(path);
}

std::vector<std::unique_ptr<perturbations::IPerturbation>> CliParser::buildPerturbations(
    const manifests::ChaosManifest& manifest) const {
    perturbations::PerturbationFactory factory;
    std::vector<std::unique_ptr<perturbations::IPerturbation>> instances;
    instances.reserve(manifest.perturbations.size());
    for (const auto& spec : manifest.perturbations) {
        instances.push_back(factory.create(m_engine, manifest.target, spec));
    }
    return instances;
}

shared::TargetState CliParser::runPerturbationsLoop(
    std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations, const std::string& targetId,
    std::chrono::seconds duration) const {
    perturbations::PerturbationEngine pert_engine;
    pert_engine.scheduleAllAsync(std::move(perturbations), duration);

    observability::ObservabilityEngine obs(m_engine);
    shared::TargetState lastKnownState{};
    shared::StateBroadcaster broadcaster;

    SignalHandlerGuard signal_guard;

    if (duration.count() == 0) {
        lastKnownState = obs.observe(targetId);
        broadcaster.broadcast(lastKnownState);
    } else {
        const auto end_time = std::chrono::steady_clock::now() + duration;

        while (std::chrono::steady_clock::now() < end_time) {
            int dummy;
            ssize_t n = ::read(signal_guard.readEnd(), &dummy, sizeof(dummy));
            if (n > 0) {
                g_stop_source.request_stop();
                break;
            }
            if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // No data available — this is normal for non-blocking pipe
            }

            lastKnownState = obs.observe(targetId);
            broadcaster.broadcast(lastKnownState);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    if (g_stop_source.get_token().stop_requested()) {
        SPDLOG_WARN("Interrupt received; canceling perturbations.");
        pert_engine.cancel();
    }

    pert_engine.waitForTeardown();
    return lastKnownState;
}

bool CliParser::validateExpectations(const manifests::ChaosManifest& manifest,
                                     const shared::TargetState& finalState) const {
    if (manifest.expectations.empty()) {
        return true;
    }

    SPDLOG_INFO("Evaluating {} expectation(s)...", manifest.expectations.size());
    const auto results = validation::validate(finalState, manifest.expectations);

    for (const auto& result : results) {
        if (!result.passed) {
            SPDLOG_ERROR("Chaos run FAILED: one or more expectations were not met.");
            return false;
        }
    }
    SPDLOG_INFO("Chaos run PASSED: all expectations met.");
    return true;
}

}  // namespace chaos::orchestrator::interfaces::cli
