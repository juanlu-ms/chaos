/**
 * @file ValidationEngine.hpp
 * @brief Evaluates manifest-defined expectations against observed container state.
 */

#pragma once

#include <vector>

#include "../core/TargetState.hpp"
#include "../manifests/Manifest.hpp"
#include "ValidationResult.hpp"

namespace chaos::orchestrator::validation {

/**
 * @brief Evaluate all expectations against the current container state.
 * @param targetState The observed state of the target container to validate against.
 * @param expectations List of expectations from the manifest.
 * @return One ValidationResult per expectation, in order.
 *
 * Supports expectation types:
 * - container_running / container_not_running
 * - log_contains / log_not_contains
 * - http_status / http_latency
 */
[[nodiscard]] std::vector<ValidationResult> validate(const core::TargetState& targetState,
                                                     const std::vector<manifests::Expectation>& expectations);

}  // namespace chaos::orchestrator::validation
