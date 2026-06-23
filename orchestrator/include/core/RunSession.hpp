/**
 * @file RunSession.hpp
 * @brief Shared mutable state for a single chaos run.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stop_token>
#include <string>

#include "core/SharedState.hpp"

namespace chaos::orchestrator::core {

using json = nlohmann::json;

/**
 * @brief Shared mutable state for a single chaos run, observable via SSE.
 */
struct RunSession {
    /** @brief Populated by ObservationLoop, read at finalize. */
    core::SharedState state;

    /** @brief Mutex protecting all non-atomic fields in this session. */
    std::mutex mtx;
    /** @brief Condition variable for signalling state changes to waiters. */
    std::condition_variable cv;
    /** @brief Whether the run is currently in progress. */
    bool running = false;
    /** @brief Whether the run has finished (success or failure). */
    bool complete = false;
    /** @brief JSON-serializable results and per-expectation details. */
    json results;
    /** @brief Error message populated when the run fails. */
    std::string error;
    /** @brief Stop source for requesting cancellation of background work. */
    std::stop_source stop_source;
    /** @brief Whether the run was aborted by the user. */
    std::atomic<bool> abort_requested{false};
};

}  // namespace chaos::orchestrator::core
