#include "interfaces/cli/CliParser.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
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
#include "interfaces/cli/BashCompletionGenerator.hpp"
#include "interfaces/cli/CliSpec.hpp"
#include "interfaces/cli/ProgressCoordinator.hpp"
#include "interfaces/cli/UsageRenderer.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/OtlpRunObserver.hpp"
#include "signals/SignalHandlerGuard.hpp"

namespace chaos::orchestrator::interfaces::cli {

namespace {

[[nodiscard]] bool isVerboseFlag(const std::string_view arg) { return arg == "--verbose" || arg == "-v"; }

[[nodiscard]] bool isQuietFlag(const std::string_view arg) { return arg == "--quiet" || arg == "-q"; }

[[nodiscard]] bool isLogLevelFlag(const std::string_view arg) { return arg == "--log-level"; }

[[nodiscard]] bool isNoColorFlag(const std::string_view arg) { return arg == "--no-color"; }

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
        std::size_t frame_index;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            frame_index_ = (frame_index_ + 1) % kFrames.size();
            phase = phase_;
            cpu = cpu_;
            mem = mem_;
            frame_index = frame_index_;
        }
        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
        std::string line =
            fmt::format("{} {}  {:.1f}s  cpu={:.1f}%  mem={:.1f}MB", kFrames[frame_index], phase, elapsed, cpu, mem);
        ProgressCoordinator::instance().renderLine(fmt::format("\r{: <80}", line));
    }

    static constexpr std::array<const char*, 10> kFrames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

    bool enabled_;
    std::jthread thread_;
    std::mutex dataMutex_;
    std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
    std::size_t frame_index_{0};
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
            result.options.log_level = spdlog::level::debug;
        } else if (isQuietFlag(arg)) {
            result.options.log_level = spdlog::level::warn;
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
            result.options.log_level = level.value();
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
                     std::shared_ptr<history::IRunHistory> history, std::optional<std::string> otlp_endpoint)
    : engine_(std::move(engine)),
      history_(std::move(history)),
      service_(engine_),
      otlp_endpoint_(std::move(otlp_endpoint)) {}

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

    const CommandSpec* spec = findCommand(command);
    if (spec == nullptr) {
        fmt::print(stderr, "Unknown command: '{}'\n", command);
        printUsage();
        return 1;
    }

    switch (spec->commandId) {
        case CommandId::Help:
            printUsage();
            return 0;

        case CommandId::List:
            return handleList();

        case CommandId::Stop:
            if (args.size() < 3) {
                fmt::print(stderr, "'stop' requires a container ID\n");
                return 1;
            }
            return handleStop(args[2]);

        case CommandId::Kill:
            if (args.size() < 3) {
                fmt::print(stderr, "'kill' requires a container ID\n");
                return 1;
            }
            return handleKill(args[2]);

        case CommandId::Run: {
            RunOptions options;
            std::optional<std::string> manifest_path;
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
                } else if (manifest_path.has_value()) {
                    fmt::print(stderr, "Unexpected argument: {}\n", arg);
                    return 1;
                } else {
                    manifest_path = arg;
                }
            }
            if (!manifest_path.has_value()) {
                fmt::print(stderr, "'run' requires a path to a manifest JSON\n");
                return 1;
            }
            return handleRun(manifest_path.value(), options);
        }

        case CommandId::History:
            return handleHistory(args);

        case CommandId::Completion:
            return handleCompletion(args);

        case CommandId::Serve:
            fmt::print(stderr, "'serve' is handled by the chaos entry point and cannot run through the parser\n");
            return 1;
    }

    return 1;
}

void CliParser::printUsage() const { fmt::print(stdout, "{}", renderUsage()); }

int CliParser::handleCompletion(const std::vector<std::string>& args) const {
    if (args.size() < 3) {
        fmt::print(stderr, "'completion' requires a shell name ({})\n", fmt::join(kCompletionShells, ", "));
        return 1;
    }

    if (std::ranges::find(kCompletionShells, args[2]) == kCompletionShells.end()) {
        fmt::print(stderr, "Unsupported shell: '{}' (supported: {})\n", args[2], fmt::join(kCompletionShells, ", "));
        return 1;
    }

    fmt::print(stdout, "{}", generateBashCompletion());
    return 0;
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
            std::string display_id = container.id;
            if (display_id.empty()) {
                display_id = "<missing>";
            } else if (display_id.size() > 12) {
                display_id = display_id.substr(0, 12);
            }
            fmt::print(stdout, "{:<14} {:<30} {}\n", display_id, container.name, container.state);
        }
    } catch (const containers::ContainerEngineError& ex) {
        fmt::print(stderr, "Failed to list containers: {}\n", ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleStop(const std::string& container_id) const {
    try {
        engine_->stopContainer(container_id);
        fmt::print(stdout, "Container {} stopped.\n", container_id);
    } catch (const std::invalid_argument& ex) {
        fmt::print(stderr, "Failed to stop container {}: {}\n", container_id, ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        fmt::print(stderr, "Failed to stop container {}: {}\n", container_id, ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleKill(const std::string& container_id) const {
    try {
        engine_->killContainer(container_id);
        fmt::print(stdout, "Container {} killed.\n", container_id);
    } catch (const std::invalid_argument& ex) {
        fmt::print(stderr, "Failed to kill container {}: {}\n", container_id, ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        fmt::print(stderr, "Failed to kill container {}: {}\n", container_id, ex.what());
        return 1;
    }

    return 0;
}

int CliParser::handleRun(const std::string& manifest_path, const RunOptions& options) const {
    std::optional<history::RunRecorder> recorder;
    std::optional<observability::OtlpRunObserver> otlp_observer;
    try {
        auto manifest = service_.parseManifest(manifest_path);
        SPDLOG_INFO("Executing manifest '{}' against target '{}'", manifest.test_name, manifest.target.id);

        auto perturbation_instances = service_.buildPerturbations(manifest);

        core::SharedState state;

        const bool json_mode =
            options.jsonOutput || (options.outputPath.has_value() && options.outputPath.value() == "-");
        ProgressRenderer renderer{!json_mode};
        CliRunObserver observer(renderer);
        recorder.emplace(manifest);
        if (otlp_endpoint_.has_value()) {
            otlp_observer.emplace(observability::OtlpExporter(*otlp_endpoint_));
        }
        std::vector<core::IRunObserver*> observer_list = {&observer, &*recorder};
        if (otlp_observer.has_value()) {
            observer_list.push_back(&*otlp_observer);
        }
        core::CompositeRunObserver composite(observer_list);

        core::ObservationLoop::Config loop_config;
        loop_config.metricsInterval = std::chrono::milliseconds(200);
        loop_config.logsInterval = std::chrono::milliseconds(2000);
        loop_config.continuous_expectations = manifest.expectations;

        signals::SignalHandlerGuard signal_guard;

        core::ObservationLoop obs_loop(engine_, manifest.target.id, state, composite, loop_config);
        obs_loop.start(signal_guard.token());

        core::RunOrchestrator orchestrator(service_);
        auto run_result =
            orchestrator.run(manifest, state, composite, std::move(perturbation_instances), signal_guard.token());

        // Observation must end before anything is rendered: the verdict was already snapshotted
        // inside run(), so later ticks could only emit warnings the report cannot account for.
        obs_loop.stop();
        renderer.finish();
        if (history_) {
            history_->save(recorder->finalize(run_result, "completed", ""));
        }
        if (otlp_observer.has_value()) {
            (void)otlp_observer->finalize(run_result);
        }
        const auto elapsed = std::chrono::duration<double>(run_result.duration_s);

        const auto json_report = core::runResultToJson(run_result).dump(2);
        const bool write_file = options.outputPath.has_value() && options.outputPath.value() != "-";

        if (write_file) {
            std::ofstream out(options.outputPath.value());
            if (!out) {
                fmt::print(stderr, "Failed to open output file: {}\n", options.outputPath.value());
                return 1;
            }
            out << json_report << '\n';
            if (!out.good()) {
                fmt::print(stderr, "Failed to write output file: {}\n", options.outputPath.value());
                return 1;
            }
        }

        if (json_mode) {
            fmt::print(stdout, "{}\n", json_report);
        } else {
            printRunResults(run_result, manifest, elapsed);
        }

        if (!run_result.passed) {
            return 1;
        }
    } catch (const manifests::ManifestParserError& ex) {
        if (history_ && recorder.has_value()) {
            history_->save(recorder->finalize({}, "error", ex.what()));
        }
        if (otlp_observer.has_value()) {
            (void)otlp_observer->finalize({});
        }
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    } catch (const std::invalid_argument& ex) {
        if (history_ && recorder.has_value()) {
            history_->save(recorder->finalize({}, "error", ex.what()));
        }
        if (otlp_observer.has_value()) {
            (void)otlp_observer->finalize({});
        }
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    } catch (const containers::ContainerEngineError& ex) {
        if (history_ && recorder.has_value()) {
            history_->save(recorder->finalize({}, "error", ex.what()));
        }
        if (otlp_observer.has_value()) {
            (void)otlp_observer->finalize({});
        }
        fmt::print(stderr, "Failed to run manifest: {}\n", ex.what());
        return 1;
    } catch (const std::system_error& ex) {
        if (history_ && recorder.has_value()) {
            history_->save(recorder->finalize({}, "error", ex.what()));
        }
        if (otlp_observer.has_value()) {
            (void)otlp_observer->finalize({});
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

    bool json_mode = false;
    std::optional<std::string> run_id;

    for (std::size_t i = 2; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--json") {
            json_mode = true;
        } else if (!run_id.has_value()) {
            run_id = arg;
        } else {
            fmt::print(stderr, "Unexpected argument: {}\n", arg);
            return 1;
        }
    }

    if (run_id.has_value()) {
        if (run_id.value() == "clear") {
            history_->clear();
            if (json_mode) {
                fmt::print(stdout, "{{\"status\":\"cleared\"}}\n");
            } else {
                fmt::print(stdout, "Run history cleared.\n");
            }
            return 0;
        }

        auto record = history_->get(run_id.value());
        if (!record.has_value()) {
            fmt::print(stderr, "Run '{}' not found.\n", run_id.value());
            return 1;
        }

        if (json_mode) {
            fmt::print(stdout, "{}\n", history::runRecordToJson(record.value()).dump(2));
        } else {
            const auto& sum = record->summary;
            fmt::print(stdout, "\nRun: {}\n", sum.id);
            fmt::print(stdout, "Status: {}\n", sum.status);
            fmt::print(stdout, "Test:   {}\n", sum.run_result.manifest_name);
            fmt::print(stdout, "Target: {}\n", sum.run_result.target_id);
            if (!sum.error.empty()) {
                fmt::print(stdout, "Error:  {}\n", sum.error);
            }
            fmt::print(stdout, "Result: {}\n", sum.run_result.passed ? "PASS" : "FAIL");
            if (!sum.run_result.results.empty()) {
                fmt::print(stdout, "Expectations:\n");
                for (const auto& res : sum.run_result.results) {
                    const char* symbol = res.passed ? "✓" : "✗";
                    fmt::print(stdout, "  {} {}: {}\n", symbol, res.expectation_type, res.message);
                }
            }
        }
        return 0;
    }

    auto summaries = history_->list();
    if (summaries.empty()) {
        if (json_mode) {
            fmt::print(stdout, "[]\n");
        } else {
            fmt::print(stdout, "No runs yet.\n");
        }
        return 0;
    }
    if (json_mode) {
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
            std::array<char, 20> date_buf{};
            std::strftime(date_buf.data(), date_buf.size(), "%Y-%m-%d %H:%M:%S", &tm_val);

            fmt::print(stdout, "{:<18} {:<20} {:<12} {:<10} {}\n", summ.id, date_buf.data(),
                       summ.run_result.manifest_name.size() > 10 ? summ.run_result.manifest_name.substr(0, 10)
                                                                 : summ.run_result.manifest_name,
                       summ.status, summ.run_result.passed ? "PASS" : "FAIL");
        }
    }

    return 0;
}

void CliParser::printRunResults(const core::RunResult& run_result, const manifests::ChaosManifest& manifest,
                                std::chrono::duration<double> elapsed) const {
    fmt::print(stdout, "\nManifest: {}  Target: {}\n", manifest.test_name, manifest.target.id);

    std::size_t max_type_width = 0;
    for (const auto& result : run_result.results) {
        max_type_width = std::max(max_type_width, result.expectation_type.size());
    }

    std::size_t passed_count = 0;
    std::size_t failed_count = 0;
    for (const auto& result : run_result.results) {
        const char* symbol = result.passed ? "✓" : "✗";
        fmt::print(stdout, "  {} {:<{}}  {}\n", symbol, result.expectation_type, max_type_width, result.message);
        if (result.passed) {
            ++passed_count;
        } else {
            ++failed_count;
        }
    }

    const char* verdict = run_result.passed ? "PASS" : "FAIL";
    fmt::print(stdout, "\nResult: {} ({} passed, {} failed) in {:.1f}s\n", verdict, passed_count, failed_count,
               elapsed.count());
}

}  // namespace chaos::orchestrator::interfaces::cli
