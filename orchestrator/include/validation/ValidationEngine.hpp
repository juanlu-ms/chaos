/**
 * @file ValidationEngine.hpp
 * @brief Evaluates manifest-defined expectations against observed container state.
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "manifests/Manifest.hpp"
#include "observability/ObservabilityEngine.hpp"

namespace chaos::orchestrator::validation {

/**
 * @brief Result of evaluating a single expectation.
 */
struct ValidationResult {
    /** @brief True if the expectation was met. */
    bool passed;
    /** @brief The type of expectation evaluated. */
    std::string expectationType;
    /** @brief Explanatory message for the result. */
    std::string message;
};

/**
 * @brief Evaluates a list of manifest Expectations using an ObservabilityEngine.
 *
 * Supports expectation types:
 * - container_running
 * - container_not_running
 * - log_contains
 * - log_not_contains
 * - http_status
 * - http_latency
 */
class ValidationEngine {
public:
    /**
     * @brief Construct a new ValidationEngine.
     * @param targetState Reference to the target state for querying.
     */
    explicit ValidationEngine(const shared::TargetState& targetState);

    /**
     * @brief Evaluate all expectations against the current container state.
     * @param expectations List of expectations from the manifest.
     * @return One ValidationResult per expectation, in order.
     */
    std::vector<ValidationResult> validate(const std::vector<manifests::Expectation>& expectations) const;

private:
    const shared::TargetState& targetState;
};

}  // namespace chaos::orchestrator::validation
