/**
 * @file IValidation.hpp
 * @brief Interfaces and context for validation strategies.
 */

#pragma once

#include "ValidationResult.hpp"
#include "core/TargetState.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::validation {

/**
 * @brief Strategy interface for evaluating a single expectation.
 */
class IValidation {
public:
    /** @brief Virtual destructor. */
    virtual ~IValidation() = default;

    /**
     * @brief Evaluate the given expectation using the provided context.
     * @param target_state The current state of the target container.
     * @param expectation The expectation to validate against.
     * @return A ValidationResult indicating pass/fail and diagnostic info.
     */
    virtual ValidationResult validate(const core::TargetState& target_state,
                                      const manifests::Expectation& expectation) const = 0;
};

}  // namespace chaos::orchestrator::validation
