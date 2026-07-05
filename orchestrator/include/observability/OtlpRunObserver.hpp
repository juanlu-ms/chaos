#pragma once

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "core/IRunObserver.hpp"
#include "core/RunResult.hpp"
#include "observability/OtlpExporter.hpp"

namespace chaos::orchestrator {
namespace core {
struct TargetState;
}  // namespace core

namespace observability {

/**
 * @brief Reads the OTLP HTTP endpoint from the environment variable `CHAOS_OTLP_ENDPOINT`
 *        (e.g. "http://localhost:4318").
 *
 * @return The endpoint URL if the environment variable is set and non-empty; std::nullopt
 *         otherwise, indicating that OTLP export is disabled.
 */
inline std::optional<std::string> resolveOtlpEndpoint() {
    const auto env = std::getenv("CHAOS_OTLP_ENDPOINT");
    if (env != nullptr && env[0] != '\0') {
        return std::string(env);
    }
    return std::nullopt;
}

/**
 * @brief Observer that forwards chaos run lifecycle events to an OTLP-compatible backend.
 *
 * Implements IRunObserver.  When a run is active this observer:
 * - Exports every phase transition immediately.
 * - Exports state samples throttled to at most one per second.
 * - Exports a final run-complete (or run-failed) summary on finalize().
 *
 * All callbacks are noexcept.  Export failures are logged at WARN level and swallowed;
 * they never propagate to the caller.
 *
 * When the CHAOS_OTLP_ENDPOINT environment variable is not set the observer
 * should not be instantiated, incurring zero cost.
 */
class OtlpRunObserver final : public core::IRunObserver {
public:
    /**
     * @brief Construct an OTLP run observer backed by the given exporter.
     *
     * Generates a unique run_id from the current system clock (milliseconds since epoch,
     * prefixed with "run-").  Initializes the throttle timer to epoch so the first state
     * sample is exported immediately.
     *
     * @param exporter A configured OtlpExporter instance (moved into the observer).
     */
    explicit OtlpRunObserver(OtlpExporter exporter);

    // ── IRunObserver overrides ─────────────────────────────────────

    /**
     * @brief Export a phase-change event immediately (not throttled).
     *
     * Severity: "phase_change".  Body: {"phase":"\<phase\>"}.
     *
     * @param phase The new phase name (e.g. "normal", "chaos", "recovery").
     */
    void onPhaseChange(std::string_view phase) noexcept override;

    /**
     * @brief Export a state-sample event, throttled to at most once per second.
     *
     * Severity: "state_sample".  Body: {"cpu_percent":\<n\>,"memory_mb":\<n\>,
     * "net_rx_bps":\<n\>,"net_tx_bps":\<n\>,"phase":"\<phase\>","container_id":"\<id\>"}.
     *
     * @param state The latest observed target state.
     */
    void onStateUpdate(const core::TargetState& state) noexcept override;

    /**
     * @brief Log updates are intentionally not exported to OTLP (no-op).
     *
     * Container logs are high-volume and would stall the synchronous HTTP exporter.
     */
    void onLogsUpdate(const std::vector<std::string>& /*logs*/) noexcept override {}

    /**
     * @brief Network latency is included in state-sample bodies (no-op).
     */
    void onNetworkLatencyUpdate(std::optional<double> /*latency*/) noexcept override {}

    // ── Run completion ──────────────────────────────────────────────

    /**
     * @brief Export the final run result and return whether the export succeeded.
     *
     * Severity: "run_complete" (pass) or "run_failed" (fail).
     * Body: {"passed":\<bool\>,"failures":\<n\>,"duration_s":\<n\>,"manifest":"\<name\>"}.
     *
     * Must be called exactly once, before the observer is destroyed.  Idempotency is
     * not guaranteed (the caller is expected to call it once).
     *
     * @param result The finalized run result from ChaosService.
     * @return true if the OTLP backend acknowledged the export.
     */
    [[nodiscard]] bool finalize(const core::RunResult& result);

private:
    OtlpExporter exporter_;
    std::string run_id_;
    std::string phase_;
    std::chrono::steady_clock::time_point last_state_export_;
    static constexpr std::chrono::milliseconds state_throttle_{1000};
    mutable std::mutex mutex_;
};

}  // namespace observability
}  // namespace chaos::orchestrator
