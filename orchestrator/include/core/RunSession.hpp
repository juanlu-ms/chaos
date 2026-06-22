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
    core::SharedState state;  // populated by ObservationLoop, read at finalize

    std::mutex mtx;
    std::condition_variable cv;
    bool running = false;
    bool complete = false;
    json results;
    std::string error;
    std::stop_source stop_source;
    std::atomic<bool> abortRequested{false};
};

}  // namespace chaos::orchestrator::core
