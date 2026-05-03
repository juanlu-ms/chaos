#pragma once

#include <functional>
#include <mutex>
#include <vector>

#include "shared/TargetState.hpp"

namespace chaos::orchestrator::shared {

/**
 * @brief Thread-safe broadcaster for target state updates.
 */
class StateBroadcaster {
public:
    using StateCallback = std::function<void(const TargetState&)>;

    /**
     * @brief Register a subscriber to receive state updates.
     * @param callback Callback invoked on broadcast.
     */
    void subscribe(StateCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.push_back(std::move(callback));
    }

    /**
     * @brief Broadcast a state update to all subscribers.
     * @param state Latest observed target state.
     */
    void broadcast(const TargetState& state) {
        std::vector<StateCallback> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = subscribers_;
        }

        for (const auto& sub : snapshot) {
            sub(state);
        }
    }

private:
    std::mutex mutex_;
    std::vector<StateCallback> subscribers_;
};

}  // namespace chaos::orchestrator::shared
