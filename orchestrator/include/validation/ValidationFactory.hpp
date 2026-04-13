/**
 * @file ValidationFactory.hpp
 * @brief Interfaces for instantiating expectation validators.
 */

#pragma once

#include <memory>

#include "IValidation.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::validation {

/**
 * @brief Factory for creating validation strategies.
 */
class ValidationFactory final {
public:
    ValidationFactory() = default;
    ~ValidationFactory() = default;

    std::unique_ptr<IValidation> create(const manifests::Expectation& expectation);
};

}  // namespace chaos::orchestrator::validation
