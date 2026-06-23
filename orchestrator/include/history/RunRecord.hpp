/**
 * @file RunRecord.hpp
 * @brief Data structures for persisting and recalling chaos run history.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/RunResult.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::history {

/**
 * @brief A single observation sample captured at a point in time during a run.
 */
struct RunSample {
    /** @brief Seconds since run start (steady clock). */
    double t{0.0};
    /** @brief CPU usage percentage, if available. */
    std::optional<double> cpu_usage_percent;
    /** @brief Memory usage in MB, if available. */
    std::optional<double> memory_usage_mb;
    /** @brief Network receive rate in bytes per second, if available. */
    std::optional<double> network_rx_bps;
    /** @brief Network transmit rate in bytes per second, if available. */
    std::optional<double> network_tx_bps;
    /** @brief Network latency in milliseconds, if available. */
    std::optional<double> network_latency_ms;
    /** @brief Current run phase: "normal", "chaos", or "recovery". */
    std::string phase;
};

/**
 * @brief High-level metadata about a completed, aborted, or failed run.
 */
struct RunSummary {
    /** @brief Unique run identifier in the form "run-<unix_ms>". */
    std::string id;
    /** @brief Unix epoch milliseconds when the run started — used as sort key and filename. */
    int64_t started_at_unix{0};
    /** @brief Unix epoch milliseconds when the run ended. */
    int64_t ended_at_unix{0};
    /** @brief Final run status: "completed", "aborted", or "error". */
    std::string status;
    /** @brief Error description, non-empty only on error or abort. */
    std::string error;
    /** @brief List of perturbation types that were applied. */
    std::vector<std::string> perturbation_types;
    /** @brief Seconds from start when the normal phase ended. */
    double normal_end_t{0.0};
    /** @brief Seconds from start when the chaos phase ended. */
    double chaos_end_t{0.0};
    /** @brief Embedded final run result carrying pass/fail and validation details. */
    core::RunResult run_result;
};

/**
 * @brief Complete record of a single chaos test run, combining summary, manifest, telemetry samples, and logs.
 */
struct RunRecord {
    /** @brief Summary metadata for the run. */
    RunSummary summary;
    /** @brief The chaos test manifest that drove this run. */
    manifests::ChaosManifest manifest;
    /** @brief Time-series observation samples collected during the run. */
    std::vector<RunSample> samples;
    /** @brief Final tail of container logs (at most 50 lines). */
    std::vector<std::string> logs;
};

}  // namespace chaos::orchestrator::history
