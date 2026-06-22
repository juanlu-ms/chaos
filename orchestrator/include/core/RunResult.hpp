/**
 * @file RunResult.hpp
 * @brief Result type for a completed chaos test run.
 */

#pragma once

#include <string>
#include <vector>

#include "validation/ValidationResult.hpp"

namespace chaos::orchestrator::core {

/**
 * @brief Result of running and validating a chaos test.
 */
struct RunResult {
    /** @brief Whether the run passed all validations. */
    bool passed{false};
    /** @brief Individual validation results for each target. */
    std::vector<validation::ValidationResult> results;
    /** @brief Manifest test name, copied for reporting. */
    std::string manifest_name;
    /** @brief Target container identifier, copied for reporting. */
    std::string target_id;
    /** @brief Wall-clock run duration in seconds. */
    double duration_s{0.0};
    /** @brief ISO-8601 UTC timestamp of run start. */
    std::string started_at;
};

}  // namespace chaos::orchestrator::core
