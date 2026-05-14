/**
 * @file JsonSerializer.hpp
 * @brief JSON serialization helpers for the web interface.
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "containers/SystemInfo.hpp"
#include "shared/TargetState.hpp"

namespace chaos::orchestrator::interfaces::web {

using json = nlohmann::json;

/**
 * @brief Serialize a TargetState to JSON for SSE streaming.
 * @param state The observed container state.
 * @param phase Optional phase label (e.g. "normal", "chaos", "recovery").
 */
json stateToJson(const shared::TargetState& state, const std::string& phase = "");

/**
 * @brief Serialize system limits to JSON for the /api/limits endpoint.
 */
json limitsToJson(const containers::SystemInfo& info);

/**
 * @brief Split raw log text into individual lines, stripping CR.
 */
void parseLogLines(const std::string& raw, std::vector<std::string>& out);

}  // namespace chaos::orchestrator::interfaces::web
