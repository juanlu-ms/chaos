/**
 * @file HttpValidation.hpp
 * @brief Validation strategies for HTTP expectation checks.
 */

#pragma once

#include "validation/IValidation.hpp"

namespace chaos::orchestrator::validation {

class HttpStatusValidation final : public IValidation {
public:
    ValidationResult validate(const core::TargetState& targetState,
                              const manifests::Expectation& expectation) const override;
};

class HttpLatencyValidation final : public IValidation {
public:
    ValidationResult validate(const core::TargetState& targetState,
                              const manifests::Expectation& expectation) const override;
};

}  // namespace chaos::orchestrator::validation
