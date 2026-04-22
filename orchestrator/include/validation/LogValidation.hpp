/**
 * @file LogValidation.hpp
 * @brief Validation strategies for log content expectations.
 */

#pragma once

#include "IValidation.hpp"

namespace chaos::orchestrator::validation {

class LogContainsValidation final : public IValidation {
public:
    ValidationResult validate(const shared::TargetState& targetState,
                              const manifests::Expectation& expectation) const override;
};

class LogNotContainsValidation final : public IValidation {
public:
    ValidationResult validate(const shared::TargetState& targetState,
                              const manifests::Expectation& expectation) const override;
};

}  // namespace chaos::orchestrator::validation
