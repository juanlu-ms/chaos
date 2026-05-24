/**
 * @file HttpValidatorBase.hpp
 * @brief Shared HTTP validation logic extracted from HttpStatusValidation and HttpLatencyValidation.
 */

#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "manifests/Manifest.hpp"
#include "shared/TargetState.hpp"
#include "validation/ValidationResult.hpp"

namespace chaos::orchestrator::validation::detail {

// Callback for the domain-specific check inside performHttpValidation.
// Receives HTTP status code and elapsed time in ms after a successful GET.
// Sets passed and message on the result.
using CheckFn = std::function<void(int statusCode, double elapsedMs, ValidationResult& result)>;

// Perform common HTTP validation setup: parse params, create client, execute GET, call check.
// Extracts "url", "expected_status", and "timeout_ms" from the expectation parameters,
// creates an httplib::Client, performs a GET request, measures elapsed time,
// and delegates the domain-specific check to checkFn.
ValidationResult performHttpValidation(const shared::TargetState& targetState,
                                       const manifests::Expectation& expectation,
                                       CheckFn checkFn);

}  // namespace chaos::orchestrator::validation::detail
