/**
 * @file ObservationLoopDetail.hpp
 * @brief Testable helpers extracted from ObservationLoop for metrics parsing and latency measurement.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string_view>

namespace chaos::orchestrator::core::detail {

// Network metrics parsed from /proc/{pid}/net/dev.
struct NetworkMetrics {
    double rx_bps = 0;  // Received bytes per second.
    double tx_bps = 0;  // Transmitted bytes per second.
};

// Parse network stats from a container process's /proc/{pid}/net/dev.
// Reads cumulative RX/TX byte counters and computes B/s delta against
// the previous snapshot. Updates in-out parameters for the next call.
NetworkMetrics parseProcNetDev(int pid, uint64_t& prev_rx, uint64_t& prev_tx,
                               std::chrono::steady_clock::time_point& prev_time, bool& prev_valid);

// Execute a single ICMP ping to measure network latency.
// Returns RTT in milliseconds, or nullopt on failure.
[[nodiscard]] std::optional<double> executePing(std::string_view target_ip);

// Sleep for the given duration in short slices, returning early once stop is
// requested. Takes milliseconds so sub-second waits are honoured; seconds
// convert implicitly.
void interruptibleSleep(std::chrono::milliseconds duration, const std::stop_token& stop);

// As above, but wakes on whichever token is requested first, so a caller
// watching both an internal and an external stop is not held by the sleep
// after either one fires.
void interruptibleSleep(std::chrono::milliseconds duration, const std::stop_token& first,
                        const std::stop_token& second);

}  // namespace chaos::orchestrator::core::detail
