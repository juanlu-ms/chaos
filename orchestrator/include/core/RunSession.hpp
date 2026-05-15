/**
 * @file RunSession.hpp
 * @brief Shared mutable state for a single chaos run.
 */

#pragma once

#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "shared/TargetState.hpp"

namespace chaos::orchestrator::core {

using json = nlohmann::json;

/**
 * @brief Shared mutable state for a single chaos run, observable via SSE.
 */
struct RunSession {
    std::mutex mtx;
    std::condition_variable cv;
    std::optional<shared::TargetState> latest;
    std::vector<std::string> pending_logs;
    bool running = false;
    bool complete = false;
    json results;
    std::string error;
    std::string phase;
    std::stop_source stop_source;
};

}  // namespace chaos::orchestrator::core
