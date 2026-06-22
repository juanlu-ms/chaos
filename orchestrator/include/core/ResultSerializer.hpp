/**
 * @file ResultSerializer.hpp
 * @brief Shared serialization helpers for core result types.
 */

#pragma once

#include <nlohmann/json.hpp>

#include "core/RunResult.hpp"

namespace chaos::orchestrator::core {

/**
 * @brief Serialize a RunResult to JSON for the /api/events complete message.
 */
nlohmann::json runResultToJson(const RunResult& runResult);

}  // namespace chaos::orchestrator::core
