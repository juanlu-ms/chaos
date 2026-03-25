/// @file ValidationEngine.hpp
/// @brief Evaluates manifest-defined expectations against observed container state.

#pragma once

#include <string>
#include <vector>

#include "manifests/Manifest.hpp"
#include "observability/ObservabilityEngine.hpp"

namespace chaos::orchestrator::observability {

/**
 * @brief Result of evaluating a single expectation.
 */
struct ValidationResult {
    bool passed;
    std::string expectationType;
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
 */
class ValidationEngine {
public:
    explicit ValidationEngine(ObservabilityEngine obs);

    /**
     * @brief Evaluate all expectations against the current container state.
     * @param containerId Target container ID.
     * @param expectations List of expectations from the manifest.
     * @return One ValidationResult per expectation, in order.
     */
    std::vector<ValidationResult> validate(const std::string& containerId,
                                           const std::vector<manifests::Expectation>& expectations) const;

private:
    ObservabilityEngine obs_;
};

}  // namespace chaos::orchestrator::observability
