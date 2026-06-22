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

namespace chaos::orchestrator::interfaces::web {

using json = nlohmann::json;

/**
 * @brief Serialize a TargetState to JSON for SSE streaming.
 * @param state The observed container state.
 * @param phase Optional phase label (e.g. "normal", "chaos", "recovery").
 * @param continuous_failures Optional list of continuous expectation failures.
 * @return JSON object representing the target state.
 */
json stateToJson(const core::TargetState& state, const std::string& phase = "",
                 const std::vector<std::string>& continuous_failures = {});

/**
 * @brief Serialize system limits to JSON for the /api/limits endpoint.
 * @param info System information containing resource limits.
 * @return JSON object representing system limits.
 */
json limitsToJson(const containers::SystemInfo& info);

}  // namespace chaos::orchestrator::interfaces::web
