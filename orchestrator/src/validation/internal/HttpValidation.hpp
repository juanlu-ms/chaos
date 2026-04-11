/**
 * @file HttpValidation.hpp
 * @brief Validation strategies for HTTP expectation checks.
 */

#pragma once

#include "observability/IValidation.hpp"

namespace chaos::orchestrator::observability {

class HttpStatusValidation final : public IValidation {
public:
    ValidationResult validate(const ValidationContext& ctx, const manifests::Expectation& expectation) const override;
};

class HttpLatencyValidation final : public IValidation {
public:
    ValidationResult validate(const ValidationContext& ctx, const manifests::Expectation& expectation) const override;
};

}  // namespace chaos::orchestrator::observability
