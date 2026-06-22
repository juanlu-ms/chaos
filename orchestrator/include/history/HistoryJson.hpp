/**
 * @file HistoryJson.hpp
 * @brief JSON serialization and deserialization helpers for run history records.
 */

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace chaos::orchestrator::history {

struct RunRecord;
struct RunSummary;

/**
 * @brief Serialize a full RunRecord to a JSON object.
 */
nlohmann::json runRecordToJson(const RunRecord& record);

/**
 * @brief Serialize a RunSummary to a JSON object (includes embedded RunResult fields).
 */
nlohmann::json runSummaryToJson(const RunSummary& summary);

/**
 * @brief Deserialize a RunRecord from a JSON object.
 * @return The parsed RunRecord, or std::nullopt on parse failure.
 */
std::optional<RunRecord> runRecordFromJson(const nlohmann::json& j);

/**
 * @brief Deserialize a RunSummary from a JSON object.
 * @return The parsed RunSummary, or std::nullopt on parse failure.
 */
std::optional<RunSummary> runSummaryFromJson(const nlohmann::json& j);

}  // namespace chaos::orchestrator::history
