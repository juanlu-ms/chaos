/**
 * @file ValidationResult.hpp
 * @brief Standalone result type for expectation evaluation.
 */

#pragma once

#include <string>

namespace chaos::orchestrator::validation {

/**
 * @brief Result of evaluating a single expectation.
 */
struct ValidationResult {
    /** @brief True if the expectation was met. */
    bool passed{false};
    /** @brief The type of expectation evaluated. */
    std::string expectationType;
    /** @brief Explanatory message for the result. */
    std::string message;
};

}  // namespace chaos::orchestrator::validation
