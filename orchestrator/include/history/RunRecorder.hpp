/**
 * @file RunRecorder.hpp
 * @brief Captures time-series data during a chaos run for historical storage.
 */

#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/IRunObserver.hpp"
#include "history/RunRecord.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::history {

/**
 * @brief Captures time-series data during a chaos run for historical storage.
 *
 * Implements IRunObserver. All mutable state is mutex-guarded because
 * callbacks fire on multiple ObservationLoop threads (metrics, latency, logs).
 */
class RunRecorder : public core::IRunObserver {
public:
    /**
     * @brief Construct with the manifest to record.
     * @param manifest The chaos manifest (moved into the recorder).
     */
    explicit RunRecorder(manifests::ChaosManifest manifest);

    /**
     * @brief Record a state sample.
     * @param state The current target state snapshot.
     *
     * Appends a RunSample with elapsed time since construction, current metrics,
     * last known latency, and the current phase.
     */
    void onStateUpdate(const core::TargetState& state) override;

    /**
     * @brief Store the latest latency value for the next sample.
     * @param latency The measured network latency, or std::nullopt if unavailable.
     */
    void onNetworkLatencyUpdate(std::optional<double> latency) override;

    /**
     * @brief Record phase transitions and zone boundaries.
     * @param phase The new phase name ("normal", "chaos", or "recovery").
     *
     * On normal->chaos: records normal_end_t.
     * On chaos->recovery: records chaos_end_t.
     * Phase strings: "normal", "chaos", "recovery".
     */
    void onPhaseChange(std::string_view phase) override;

    /**
     * @brief Replace the stored log tail with new logs (mirrors WebRunObserver).
     * @param logs The latest log lines from the target.
     */
    void onLogsUpdate(const std::vector<std::string>& logs) override;

    /**
     * @brief Build the final RunRecord and return it.
     * @param runResult The run result from ChaosRunner.
     * @param status "completed", "aborted", or "error".
     * @param error Error message if status is "error".
     * @return The complete RunRecord with all accumulated data.
     *
     * Stamps ended_at_unix, copies runResult into summary, derives
     * perturbation_types from the manifest's perturbation types. Thread-safe.
     */
    [[nodiscard]] RunRecord finalize(const core::RunResult& runResult, std::string status, std::string error);

private:
    manifests::ChaosManifest manifest_;
    std::chrono::steady_clock::time_point start_steady_;
    std::chrono::system_clock::time_point start_system_;
    std::vector<RunSample> samples_;
    std::vector<std::string> logs_;
    std::optional<double> last_latency_;
    std::string phase_{"normal"};
    RunSummary summary_;
    mutable std::mutex mutex_;
};

}  // namespace chaos::orchestrator::history
