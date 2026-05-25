/**
 * @file ValidationFactory.hpp
 * @brief Factory function for instantiating expectation validators.
 */

#pragma once

#include <memory>

#include "IValidation.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::validation {

/**
 * @brief Create a validation strategy for the given expectation.
 * @param expectation The expectation to validate.
 * @return Unique pointer to IValidation strategy.
 * @throws std::invalid_argument If the expectation type is not recognized.
 */
std::unique_ptr<IValidation> createValidator(const manifests::Expectation& expectation);

}  // namespace chaos::orchestrator::validation
