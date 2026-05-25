/**
 * @file JsonSerializer.hpp
 * @brief JSON serialization helpers for the web interface.
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "containers/SystemInfo.hpp"
#include "core/TargetState.hpp"

namespace chaos::orchestrator::core {
struct RunResult;
}

namespace chaos::orchestrator::interfaces::web {

using json = nlohmann::json;

/**
 * @brief Serialize a TargetState to JSON for SSE streaming.
 * @param state The observed container state.
 * @param phase Optional phase label (e.g. "normal", "chaos", "recovery").
 * @param continuous_failures Optional list of continuous expectation failures.
 */
json stateToJson(const core::TargetState& state, const std::string& phase = "",
                 const std::vector<std::string>& continuous_failures = {});

/**
 * @brief Serialize system limits to JSON for the /api/limits endpoint.
 */
json limitsToJson(const containers::SystemInfo& info);

/**
 * @brief Serialize a RunResult to JSON for the /api/events complete message.
 */
json runResultToJson(const core::RunResult& runResult);

}  // namespace chaos::orchestrator::interfaces::web
