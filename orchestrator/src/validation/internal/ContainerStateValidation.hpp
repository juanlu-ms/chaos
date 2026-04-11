/**
 * @file ContainerStateValidation.hpp
 * @brief Validation strategies for container running state expectations.
 */

#pragma once

#include "observability/IValidation.hpp"

namespace chaos::orchestrator::observability {

class ContainerRunningValidation final : public IValidation {
public:
    ValidationResult validate(const ValidationContext& ctx, const manifests::Expectation& expectation) const override;
};

class ContainerNotRunningValidation final : public IValidation {
public:
    ValidationResult validate(const ValidationContext& ctx, const manifests::Expectation& expectation) const override;
};

}  // namespace chaos::orchestrator::observability
