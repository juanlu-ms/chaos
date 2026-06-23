/**
 * @file ResultSerializer.cpp
 * @brief Implementation of shared result serialization helpers.
 */

#include "core/ResultSerializer.hpp"

#include "core/ChaosRunner.hpp"
#include "validation/ValidationResult.hpp"

namespace chaos::orchestrator::core {

nlohmann::json runResultToJson(const RunResult& run_result) {
    nlohmann::json result;
    result["passed"] = run_result.passed;
    result["manifest_name"] = run_result.manifest_name;
    result["target_id"] = run_result.target_id;
    result["duration_s"] = run_result.duration_s;
    result["started_at"] = run_result.started_at;
    result["results"] = nlohmann::json::array();
    for (const auto& validation : run_result.results) {
        result["results"].push_back(
            {{"type", validation.expectation_type}, {"passed", validation.passed}, {"message", validation.message}});
    }
    return result;
}

}  // namespace chaos::orchestrator::core
