/**
 * @file IValidation.hpp
 * @brief Interfaces and context for validation strategies.
 */

#pragma once

#include "../shared/TargetState.hpp"
#include "ValidationResult.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::validation {

/**
 * @brief Strategy interface for evaluating a single expectation.
 */
class IValidation {
public:
    virtual ~IValidation() = default;

    /**
     * @brief Evaluate the given expectation using the provided context.
     */
    virtual ValidationResult validate(const shared::TargetState& targetState,
                                      const manifests::Expectation& expectation) const = 0;
};

}  // namespace chaos::orchestrator::validation
