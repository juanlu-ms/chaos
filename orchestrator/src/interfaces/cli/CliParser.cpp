#include "interfaces/cli/CliParser.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <fstream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "core/CompositeRunObserver.hpp"
#include "core/IRunObserver.hpp"
#include "core/ObservationLoop.hpp"
#include "core/ResultSerializer.hpp"
#include "core/RunOrchestrator.hpp"
#include "core/SharedState.hpp"
#include "history/HistoryJson.hpp"
#include "history/RunRecorder.hpp"
#include "interfaces/cli/ProgressCoordinator.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/OtlpRunObserver.hpp"
#include "signals/SignalHandlerGuard.hpp"

namespace chaos::orchestrator::interfaces::cli {

namespace {

[[nodiscard]] bool isVerboseFlag(const std::string_view arg) { return arg == "--verbose" || arg == "-v"; }

[[nodiscard]] bool isQuietFlag(const std::string_view arg) { return arg == "--quiet" || arg == "-q"; }

[[nodiscard]] bool isLogLevelFlag(const std::string_view arg) { return arg == "--log-level"; }

[[nodiscard]] bool isNoColorFlag(const std::string_view arg) { return arg == "--no-color"; }

[[nodiscard]] bool isHelpCommand(const std::string_view command) {
    return command == "help" || command == "--help" || command == "-h";
}

class ProgressRenderer {
public:
    explicit ProgressRenderer(bool enabled = true) : enabled_(enabled && isatty(STDERR_FILENO) != 0) {
        if (enabled_) {
            ProgressCoordinator::instance().setActive(true);
            thread_ = std::jthread([this](std::stop_token st) {
                while (!st.stop_requested()) {
                    render();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });
        }
    }

    ~ProgressRenderer() { finish(); }

    ProgressRenderer(const ProgressRenderer&) = delete;
    ProgressRenderer& operator=(const ProgressRenderer&) = delete;

    void update(const core::TargetState& state) {
        if (!enabled_) return;
        std::lock_guard<std::mutex> lock(dataMutex_);
        cpu_ = state.cpu_usage_percent.value_or(0.0);
        mem_ = state.memory_usage_mb.value_or(0.0);
    }

    void setPhase(std::string_view phase) {
        if (!enabled_) return;
        std::lock_guard<std::mutex> lock(dataMutex_);
        phase_ = phase;
    }

    void finish() {
        if (finished_.exchange(true)) return;
        if (enabled_ && thread_.joinable()) {
            thread_.request_stop();
            thread_.join();
        }
        ProgressCoordinator::instance().finish();
    }

private:
    void render() {
        std::string phase;
        double cpu;
        double mem;
        std::size_t frameIndex;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            frameIndex_ = (frameIndex_ + 1) % kFrames.size();
            phase = phase_;
            cpu = cpu_;
            mem = mem_;
            frameIndex = frameIndex_;
        }
        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
        std::string line =
            fmt::format("{} {}  {:.1f}s  cpu={:.1f}%  mem={:.1f}MB", kFrames[frameIndex], phase, elapsed, cpu, mem);
        ProgressCoordinator::instance().renderLine(fmt::format("\r{: <80}", line));
    }

    static constexpr std::array<const char*, 10> kFrames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

    bool enabled_;
    std::jthread thread_;
    std::mutex dataMutex_;
    std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
    std::size_t frameIndex_{0};
    std::string phase_{"normal"};
    double cpu_{0.0};
    double mem_{0.0};
    std::atomic<bool> finished_{false};
};

class CliRunObserver final : public core::IRunObserver {
public:
    explicit CliRunObserver(ProgressRenderer& renderer) : renderer_(renderer) {}

    void onStateUpdate(const core::TargetState& state) override { renderer_.update(state); }

    void onPhaseChange(std::string_view phase) override { renderer_.setPhase(phase); }

    void onLogsUpdate(const std::vector<std::string>& /*logs*/) override {}

    void onNetworkLatencyUpdate(std::optional<double> /*latency*/) override {}

private:
    ProgressRenderer& renderer_;
};

[[nodiscard]] std::optional<spdlog::level::level_enum> parseLevelString(std::string_view level) {
    if (level == "trace") return spdlog::level::trace;
    if (level == "debug") return spdlog::level::debug;
    if (level == "info") return spdlog::level::info;
    if (level == "warning" || level == "warn") return spdlog::level::warn;
    if (level == "error") return spdlog::level::err;
    if (level == "critical") return spdlog::level::critical;
    if (level == "off") return spdlog::level::off;
    return std::nullopt;
}

}  // namespace

ParsedGlobalFlags parseGlobalFlags(std::span<char*> argv) {
    ParsedGlobalFlags result;
    result.options.colorize = isatty(STDERR_FILENO) != 0;
    if (!argv.empty()) {
        result.args.push_back(argv[0]);
    }

    for (std::size_t i = 1; i < argv.size(); ++i) {
        std::string_view arg = argv[i];
        if (isVerboseFlag(arg)) {
            result.options.logLevel = spdlog::level::debug;
        } else if (isQuietFlag(arg)) {
            result.options.logLevel = spdlog::level::warn;
        } else if (isLogLevelFlag(arg)) {
            if (i + 1 >= argv.size()) {
                result.error = "--log-level requires a value";
                return result;
            }
            auto level = parseLevelString(argv[i + 1]);
            if (!level.has_value()) {
                result.error = fmt::format("Invalid log level: {}", argv[i + 1]);
                return result;
            }
            result.options.logLevel = level.value();
            ++i;
        } else if (isNoColorFlag(arg)) {
            result.options.colorize = false;
        } else {
            result.args.push_back(argv[i]);
        }
    }

    return result;
}

CliParser::CliParser(std::shared_ptr<containers::IContainerEngine> engine,
                     std::shared_ptr<history::IRunHistory> history, std::optional<std::string> otlpEndpoint)
    : engine_(std::move(engine)),
      history_(std::move(history)),
      runner_(engine_),
      otlpEndpoint_(std::move(otlpEndpoint)) {}

int CliParser::run(std::span<char*> argv) const {
    if (argv.size() < 2) {
        printUsage();
        return 1;
    }

    std::vector<std::string> args(argv.begin(), argv.end());
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
        RunOptions options;
        std::optional<std::string> manifestPath;
        for (std::size_t i = 2; i < args.size(); ++i) {
            const std::string& arg = args[i];
            if (arg == "--json") {
                options.jsonOutput = true;
            } else if (arg == "--output") {
                if (i + 1 >= args.size()) {
                    fmt::print(stderr, "'--output' requires a value\n");
                    return 1;
                }
                options.outputPath = args[++i];
            } else if (manifestPath.has_value()) {
                fmt::print(stderr, "Unexpected argument: {}\n", arg);
                return 1;
            } else {
                manifestPath = arg;
            }
        }
        if (!manifestPath.has_value()) {
            fmt::print(stderr, "'run' requires a path to a manifest JSON\n");
            return 1;
        }
        return handleRun(manifestPath.value(), options);
    }

    if (command == "history") {
        return handleHistory(args);
    }

    fmt::print(stderr, "Unknown command: '{}'\n", command);
    printUsage();
    return 1;
}

void CliParser::printUsage() const {
    fmt::print(stdout,
               "Usage: chaos [global-options] <command> [command-options]\n"
               "\n"
               "Chaos - Resilience Tool for Docker Containers\n"
               "\n"
               "Global Options:\n"
               "  -v, --verbose         Enable debug logging\n"
               "  -q, --quiet           Suppress non-warning logs\n"
               "      --log-level LEVEL Set log level (trace|debug|info|warn|error|critical|off)\n"
               "      --no-color        Disable colored output\n"
               "\n"
               "Commands:\n"
               "  list                  List all containers\n"
               "  stop  <container_id>  Stop a running container\n"
               "  kill  <container_id>  Kill a running container\n"
               "  run   <manifest.json> [options] Execute a chaos manifest\n"
               "  history [id] [--json] View run history or details of a specific run\n"
               "  history clear         Clear all run history\n"
               "  help                  Show this help message\n"
               "\n"
               "Run Options:\n"
               "      --json            Emit machine-readable JSON to stdout instead of the human report\n"
               "      --output PATH     Write the JSON report to PATH (use '-' for stdout)\n");
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

int CliParser::handleRun(const std::string& manifestPath, const RunOptions& options) const {
    std::optional<history::RunRecorder> recorder;
    std::optional<observability::OtlpRunObserver> otlpObserver;
    try {
        auto manifest = runner_.parseManifest(manifestPath);
        SPDLOG_INFO("Executing manifest '{}' against target '{}'", manifest.test_name, manifest.target.id);

        auto perturbation_instances = runner_.buildPerturbations(manifest);

        core::SharedState state;

        const bool jsonMode =
            options.jsonOutput || (options.outputPath.has_value() && options.outputPath.value() == "-");
        ProgressRenderer renderer{!jsonMode};
        CliRunObserver observer(renderer);
        recorder.emplace(manifest);
        if (otlpEndpoint_.has_value()) {
            otlpObserver.emplace(observability::OtlpExporter(*otlpEndpoint_));
        }
        std::vector<core::IRunObserver*> observer_list = {&observer, &*recorder};
        if (otlpObserver.has_value()) {
            observer_list.push_back(&*otlpObserver);
        }
        core::CompositeRunObserver composite(observer_list);

        core::ObservationLoop::Config loopConfig;
        loopConfig.metricsInterval = std::chrono::milliseconds(200);
        loopConfig.logsInterval = std::chrono::milliseconds(2000);
        loopConfig.continuousExpectations = manifest.expectations;

        signals::SignalHandlerGuard signal_guard;

        core::ObservationLoop obsLoop(engine_, manifest.target.id, state, composite, loopConfig);
        obsLoop.start(signal_guard.token());

        core::RunOrchestrator orchestrator(runner_);
        auto runResult =
            orchestrator.run(manifest, state, composite, std::move(perturbation_instances), signal_guard.token());
        renderer.finish();
        if (history_) {
            history_->save(recorder->finalize(runResult, "completed", ""));
        }
        if (otlpObserver.has_value()) {
            (void)otlpObserver->finalize(runResult);
        }
        const auto elapsed = std::chrono::duration<double>(runResult.duration_s);

        const auto jsonReport = core::runResultToJson(runResult).dump(2);
        const bool writeFile = options.outputPath.has_value() && options.outputPath.value() != "-";

        if (writeFile) {
            std::ofstream out(options.outputPath.value());
            if (!out) {
                fmt::print(stderr, "Failed to open output file: {}\n", options.outputPath.value());
                return 1;
            }
            out << jsonReport << '\n';
            if (!out.good()) {
                fmt::print(stderr, "Failed to write output file: {}\n", options.outputPath.value());
                return 1;
            }
        }

        if (jsonMode) {
            fmt::print(stdout, "{}\n", jsonReport);
        } else {
            printRunResults(runResult, manifest, elapsed);
        }

        if (!runResult.passed) {
            return 1;
        }
    } catch (const manifests::ManifestParserError& ex) {
        if (history_ && recorder.has_value()) {
            history_->save(recorder->finalize({}, "error", ex.what()));
        }
        if (otlpObserver.has_value()) {
            (void)otlpObserver->finalize({});
        }
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    } catch (const std::invalid_argument& ex) {
        if (history_ && recorder.has_value()) {
            history_->save(recorder->finalize({}, "error", ex.what()));
        }
        if (otlpObserver.has_value()) {
            (void)otlpObserver->finalize({});
        }
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        if (history_ && recorder.has_value()) {
            history_->save(recorder->finalize({}, "error", ex.what()));
        }
        if (otlpObserver.has_value()) {
            (void)otlpObserver->finalize({});
        }
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    } catch (const std::system_error& ex) {
        if (history_ && recorder.has_value()) {
            history_->save(recorder->finalize({}, "error", ex.what()));
        }
        if (otlpObserver.has_value()) {
            (void)otlpObserver->finalize({});
        }
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    }
    return 0;
}

int CliParser::handleHistory(const std::vector<std::string>& args) const {
    if (!history_) {
        fmt::print(stderr, "History is not available.\n");
        return 1;
    }

    bool jsonMode = false;
    std::optional<std::string> runId;

    for (std::size_t i = 2; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--json") {
            jsonMode = true;
        } else if (!runId.has_value()) {
            runId = arg;
        } else {
            fmt::print(stderr, "Unexpected argument: {}\n", arg);
            return 1;
        }
    }

    if (runId.has_value()) {
        if (runId.value() == "clear") {
            history_->clear();
            if (jsonMode) {
                fmt::print(stdout, "{{\"status\":\"cleared\"}}\n");
            } else {
                fmt::print(stdout, "Run history cleared.\n");
            }
            return 0;
        }

        auto record = history_->get(runId.value());
        if (!record.has_value()) {
            fmt::print(stderr, "Run '{}' not found.\n", runId.value());
            return 1;
        }

        if (jsonMode) {
            fmt::print(stdout, "{}\n", history::runRecordToJson(record.value()).dump(2));
        } else {
            const auto& sum = record->summary;
            fmt::print(stdout, "\nRun: {}\n", sum.id);
            fmt::print(stdout, "Status: {}\n", sum.status);
            fmt::print(stdout, "Test:   {}\n", sum.runResult.manifest_name);
            fmt::print(stdout, "Target: {}\n", sum.runResult.target_id);
            if (!sum.error.empty()) {
                fmt::print(stdout, "Error:  {}\n", sum.error);
            }
            fmt::print(stdout, "Result: {}\n", sum.runResult.passed ? "PASS" : "FAIL");
            if (!sum.runResult.results.empty()) {
                fmt::print(stdout, "Expectations:\n");
                for (const auto& res : sum.runResult.results) {
                    const char* symbol = res.passed ? "✓" : "✗";
                    fmt::print(stdout, "  {} {}: {}\n", symbol, res.expectationType, res.message);
                }
            }
        }
        return 0;
    }

    auto summaries = history_->list();
    if (summaries.empty()) {
        if (jsonMode) {
            fmt::print(stdout, "[]\n");
        } else {
            fmt::print(stdout, "No runs yet.\n");
        }
        return 0;
    }
    if (jsonMode) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& summ : summaries) {
            arr.push_back(history::runSummaryToJson(summ));
        }
        fmt::print(stdout, "{}\n", arr.dump(2));
    } else {
        fmt::print(stdout, "{:<18} {:<20} {:<12} {:<10} {}\n", "RUN ID", "DATE", "TEST", "STATUS", "RESULT");
        for (const auto& summ : summaries) {
            auto time_t_val = static_cast<std::time_t>(summ.started_at_unix / 1000);
            std::tm tm_val{};
            localtime_r(&time_t_val, &tm_val);
            std::array<char, 20> dateBuf{};
            std::strftime(dateBuf.data(), dateBuf.size(), "%Y-%m-%d %H:%M:%S", &tm_val);

            fmt::print(stdout, "{:<18} {:<20} {:<12} {:<10} {}\n", summ.id, dateBuf.data(),
                       summ.runResult.manifest_name.size() > 10 ? summ.runResult.manifest_name.substr(0, 10)
                                                                : summ.runResult.manifest_name,
                       summ.status, summ.runResult.passed ? "PASS" : "FAIL");
        }
    }

    return 0;
}

void CliParser::printRunResults(const core::RunResult& runResult, const manifests::ChaosManifest& manifest,
                                std::chrono::duration<double> elapsed) const {
    fmt::print(stdout, "\nManifest: {}  Target: {}\n", manifest.test_name, manifest.target.id);

    std::size_t maxTypeWidth = 0;
    for (const auto& result : runResult.results) {
        maxTypeWidth = std::max(maxTypeWidth, result.expectationType.size());
    }

    std::size_t passedCount = 0;
    std::size_t failedCount = 0;
    for (const auto& result : runResult.results) {
        const char* symbol = result.passed ? "✓" : "✗";
        fmt::print(stdout, "  {} {:<{}}  {}\n", symbol, result.expectationType, maxTypeWidth, result.message);
        if (result.passed) {
            ++passedCount;
        } else {
            ++failedCount;
        }
    }

    const char* verdict = runResult.passed ? "PASS" : "FAIL";
    fmt::print(stdout, "\nResult: {} ({} passed, {} failed) in {:.1f}s\n", verdict, passedCount, failedCount,
               elapsed.count());
}

}  // namespace chaos::orchestrator::interfaces::cli
