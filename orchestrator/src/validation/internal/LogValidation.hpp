/**
 * @file LogValidation.hpp
 * @brief Validation strategies for log content expectations.
 */

#pragma once

#include "observability/IValidation.hpp"

namespace chaos::orchestrator::observability {

class LogContainsValidation final : public IValidation {
public:
    ValidationResult validate(const ValidationContext& ctx, const manifests::Expectation& expectation) const override;
};

class LogNotContainsValidation final : public IValidation {
public:
    ValidationResult validate(const ValidationContext& ctx, const manifests::Expectation& expectation) const override;
};

}  // namespace chaos::orchestrator::observability
