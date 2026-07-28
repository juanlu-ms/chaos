/**
 * @file SharedState.hpp
 * @brief Thread-safe holder for the latest observed state, logs, phase, and
 *        continuous failures.
 */

#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/RunPhase.hpp"
#include "core/TargetState.hpp"

namespace chaos::orchestrator::core {

/**
 * @brief Thread-safe holder for the latest observed state, logs, phase, and
 *        continuous failures.
 *
 * Written by ObservationLoop background threads. Read by RunOrchestrator
 * at finalization. No external synchronisation required.
 */
class SharedState {
public:
    /**
     * @brief Update the latest observed target state.
     * @param state The newly observed state of the target.
     */
    void updateState(const core::TargetState& state);

    /**
     * @brief Update only metrics fields (CPU, memory, network I/O, container IP).
     *
     * Leaves network_latency_ms, logs, and phase untouched — those have
     * dedicated writers (latency thread, logs thread, RunOrchestrator).
     *
     * @param state The target state containing new metrics values.
     */
    void updateMetrics(const core::TargetState& state);

    /**
     * @brief Patch only the network latency field on the latest state.
     * @param latency RTT in milliseconds, or std::nullopt if unavailable.
     */
    void updateNetworkLatency(std::optional<double> latency);

    /**
     * @brief Get the latest observed target state.
     * @return The most recent state written via updateState().
     */
    [[nodiscard]] core::TargetState latestState() const;

    /**
     * @brief Update the latest log lines.
     * @param logs The latest log lines from the target container.
     */
    void updateLogs(const std::vector<std::string>& logs);

    /**
     * @brief Get the latest log lines.
     * @return The most recent logs written via updateLogs().
     */
    [[nodiscard]] std::vector<std::string> latestLogs() const;

    /**
     * @brief Set the current execution phase.
     * @param phase The new phase.
     */
    void setPhase(RunPhase phase);

    /**
     * @brief Get the current execution phase.
     * @return The current phase. Defaults to RunPhase::Normal before a run starts.
     */
    [[nodiscard]] RunPhase phase() const;

    /**
     * @brief Get the current execution phase as its wire name.
     * @return "normal", "chaos" or "recovery", for serialisation boundaries.
     */
    [[nodiscard]] std::string phaseName() const;

    /**
     * @brief Record a continuous expectation failure (deduplicated).
     * @param type The expectation type that failed.
     */
    void addContinuousFailure(std::string_view type);

    /**
     * @brief Get the current sequence counter value.
     * @return Monotonic counter incremented on every state/log/phase mutation.
     */
    [[nodiscard]] uint64_t sequence() const;

    /**
     * @brief Get all recorded continuous expectation failures.
     * @return List of failed expectation types.
     */
    [[nodiscard]] std::vector<std::string> continuousFailures() const;

private:
    mutable std::mutex mtx_;
    core::TargetState latest_;
    RunPhase phase_{RunPhase::Normal};
    std::vector<std::string> continuous_failures_;
    uint64_t sequence_{0};
};

inline void SharedState::updateState(const core::TargetState& state) {
    std::lock_guard lock(mtx_);
    latest_ = state;
    ++sequence_;
}

inline void SharedState::updateMetrics(const core::TargetState& state) {
    std::lock_guard lock(mtx_);
    latest_.container_id = state.container_id;
    latest_.status = state.status;
    if (state.cpu_usage_percent.has_value()) latest_.cpu_usage_percent = state.cpu_usage_percent;
    if (state.memory_usage_mb.has_value()) latest_.memory_usage_mb = state.memory_usage_mb;
    if (state.network_rx_bps.has_value()) latest_.network_rx_bps = state.network_rx_bps;
    if (state.network_tx_bps.has_value()) latest_.network_tx_bps = state.network_tx_bps;
    if (state.container_ip.has_value()) latest_.container_ip = state.container_ip;
    ++sequence_;
}

inline void SharedState::updateNetworkLatency(std::optional<double> latency) {
    std::lock_guard lock(mtx_);
    latest_.network_latency_ms = latency;
    ++sequence_;
}

inline core::TargetState SharedState::latestState() const {
    std::lock_guard lock(mtx_);
    return latest_;
}

inline void SharedState::updateLogs(const std::vector<std::string>& logs) {
    std::lock_guard lock(mtx_);
    latest_.recent_logs = logs;
    ++sequence_;
}

inline std::vector<std::string> SharedState::latestLogs() const {
    std::lock_guard lock(mtx_);
    return latest_.recent_logs;
}

inline void SharedState::setPhase(RunPhase phase) {
    std::lock_guard lock(mtx_);
    phase_ = phase;
    ++sequence_;
}

inline RunPhase SharedState::phase() const {
    std::lock_guard lock(mtx_);
    return phase_;
}

inline std::string SharedState::phaseName() const {
    std::lock_guard lock(mtx_);
    return std::string(toString(phase_));
}

inline uint64_t SharedState::sequence() const {
    std::lock_guard lock(mtx_);
    return sequence_;
}

inline void SharedState::addContinuousFailure(std::string_view type) {
    std::lock_guard lock(mtx_);
    if (std::ranges::find(continuous_failures_, type) == continuous_failures_.end()) {
        continuous_failures_.emplace_back(type);
    }
}

inline std::vector<std::string> SharedState::continuousFailures() const {
    std::lock_guard lock(mtx_);
    return continuous_failures_;
}

}  // namespace chaos::orchestrator::core
