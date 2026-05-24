/**
 * @file ObservationLoopInternal.hpp
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
    double rxBps = 0;  // Received bytes per second.
    double txBps = 0;  // Transmitted bytes per second.
};

// Parse network stats from a container process's /proc/{pid}/net/dev.
// Reads cumulative RX/TX byte counters and computes B/s delta against
// the previous snapshot. Updates in-out parameters for the next call.
NetworkMetrics parseProcNetDev(int pid, uint64_t& prevRx, uint64_t& prevTx,
                               std::chrono::steady_clock::time_point& prevTime, bool& prevValid);

// Execute a single ICMP ping to measure network latency.
// Returns RTT in milliseconds, or nullopt on failure.
[[nodiscard]] std::optional<double> executePing(std::string_view targetIp);

// Sleep in 1-second increments, checking for stop request each tick.
void interruptibleSleep(std::chrono::seconds duration, const std::stop_token& stop);

}  // namespace chaos::orchestrator::core::detail
