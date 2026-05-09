/**
 * @file ContainerStateValidation.hpp
 * @brief Validation strategies for container running state expectations.
 */

#pragma once

#include "validation/IValidation.hpp"

namespace chaos::orchestrator::validation {

class ContainerRunningValidation final : public IValidation {
public:
    ValidationResult validate(const shared::TargetState& targetState,
                              const manifests::Expectation& expectation) const override;
};

class ContainerNotRunningValidation final : public IValidation {
public:
    ValidationResult validate(const shared::TargetState& targetState,
                              const manifests::Expectation& expectation) const override;
};

}  // namespace chaos::orchestrator::validation
