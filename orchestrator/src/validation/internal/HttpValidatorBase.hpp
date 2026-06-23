/**
 * @file HttpValidatorBase.hpp
 * @brief Shared HTTP validation logic extracted from HttpStatusValidation and HttpLatencyValidation.
 */

#pragma once

#include <functional>

#include "core/TargetState.hpp"
#include "manifests/Manifest.hpp"
#include "validation/ValidationResult.hpp"

namespace chaos::orchestrator::validation::detail {

// Callback for the domain-specific check inside performHttpValidation.
// Receives HTTP status code and elapsed time in ms after a successful GET.
// Sets passed and message on the result.
using CheckFn = std::function<void(int status_code, double elapsed_ms, ValidationResult& result)>;

// Perform common HTTP validation setup: parse params, create client, execute GET, call check.
// Extracts "url", "expected_status", and "timeout_ms" from the expectation parameters,
// creates an httplib::Client, performs a GET request, measures elapsed time,
// and delegates the domain-specific check to check_fn.
ValidationResult performHttpValidation(const core::TargetState& target_state, const manifests::Expectation& expectation,
                                       CheckFn check_fn);

}  // namespace chaos::orchestrator::validation::detail
