/**
 * @file ResultSerializer.cpp
 * @brief Implementation of shared result serialization helpers.
 */

#include "core/ResultSerializer.hpp"

#include "core/ChaosRunner.hpp"
#include "validation/ValidationResult.hpp"

namespace chaos::orchestrator::core {

nlohmann::json runResultToJson(const RunResult& runResult) {
    nlohmann::json result;
    result["passed"] = runResult.passed;
    result["manifest_name"] = runResult.manifest_name;
    result["target_id"] = runResult.target_id;
    result["duration_s"] = runResult.duration_s;
    result["started_at"] = runResult.started_at;
    result["results"] = nlohmann::json::array();
    for (const auto& validation : runResult.results) {
        result["results"].push_back(
            {{"type", validation.expectationType}, {"passed", validation.passed}, {"message", validation.message}});
    }
    return result;
}

}  // namespace chaos::orchestrator::core
