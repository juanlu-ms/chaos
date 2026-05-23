#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "shared/TargetState.hpp"

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
    void updateState(const shared::TargetState& state);
    [[nodiscard]] shared::TargetState latestState() const;

    void updateLogs(const std::vector<std::string>& logs);
    [[nodiscard]] std::vector<std::string> latestLogs() const;

    void setPhase(std::string_view phase);
    [[nodiscard]] std::string phase() const;

    void addContinuousFailure(std::string_view type);
    [[nodiscard]] std::vector<std::string> continuousFailures() const;

private:
    mutable std::mutex mtx_;
    shared::TargetState latest_;
    std::vector<std::string> pending_logs_;
    std::string phase_;
    std::vector<std::string> continuous_failures_;
};

// ── Inline implementation ──────────────────────────────────────────

inline void SharedState::updateState(const shared::TargetState& state) {
    std::lock_guard lock(mtx_);
    latest_ = state;
}

inline shared::TargetState SharedState::latestState() const {
    std::lock_guard lock(mtx_);
    return latest_;
}

inline void SharedState::updateLogs(const std::vector<std::string>& logs) {
    std::lock_guard lock(mtx_);
    pending_logs_ = logs;
}

inline std::vector<std::string> SharedState::latestLogs() const {
    std::lock_guard lock(mtx_);
    return pending_logs_;
}

inline void SharedState::setPhase(std::string_view phase) {
    std::lock_guard lock(mtx_);
    phase_ = std::string(phase);
}

inline std::string SharedState::phase() const {
    std::lock_guard lock(mtx_);
    return phase_;
}

inline void SharedState::addContinuousFailure(std::string_view type) {
    std::lock_guard lock(mtx_);
    if (std::ranges::find(continuous_failures_, type) == continuous_failures_.end()) {
        continuous_failures_.push_back(std::string(type));
    }
}

inline std::vector<std::string> SharedState::continuousFailures() const {
    std::lock_guard lock(mtx_);
    return continuous_failures_;
}

}  // namespace chaos::orchestrator::core
