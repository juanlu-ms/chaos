#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#include "TargetState.hpp"

namespace chaos::orchestrator::shared {

/**
 * @brief Thread-safe broadcaster for target state updates.
 */
class StateBroadcaster {
public:
    /**
     * @brief Opaque handle returned by subscribe().
     */
    struct Handle {
        uint64_t id = 0;
        bool operator==(const Handle& o) const { return id == o.id; }
        bool operator!=(const Handle& o) const { return id != o.id; }
        explicit operator bool() const { return id != 0; }
    };

    using StateCallback = std::function<void(const TargetState&)>;

    /**
     * @brief Register a subscriber to receive state updates.
     * @param callback Callback invoked on broadcast.
     * @return Handle that can be used to unsubscribe.
     */
    Handle subscribe(StateCallback callback) {
        std::lock_guard lock(mutex_);
        auto id = ++next_id_;
        subscribers_.push_back({id, std::move(callback)});
        return Handle{id};
    }

    /**
     * @brief Remove a previously registered subscriber.
     * @param handle Handle returned by subscribe().
     */
    void unsubscribe(Handle handle) {
        std::lock_guard lock(mutex_);
        auto iter =
            std::remove_if(subscribers_.begin(), subscribers_.end(), [&](const Entry& e) { return e.id == handle.id; });
        subscribers_.erase(iter, subscribers_.end());
    }

    /**
     * @brief Broadcast a state update to all subscribers.
     * @param state Latest observed target state.
     */
    void broadcast(const TargetState& state) {
        std::vector<Entry> snapshot;
        {
            std::lock_guard lock(mutex_);
            snapshot = subscribers_;
        }

        for (const auto& entry : snapshot) {
            try {
                entry.callback(state);
            } catch (...) {
                // swallow — one bad callback doesn't starve others
            }
        }
    }

private:
    struct Entry {
        uint64_t id;
        StateCallback callback;
    };

    std::mutex mutex_;
    uint64_t next_id_ = 0;
    std::vector<Entry> subscribers_;
};

}  // namespace chaos::orchestrator::shared
