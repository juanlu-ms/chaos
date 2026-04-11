/**
 * @file ValidationFactory.hpp
 * @brief Interfaces for instantiating expectation validators.
 */

#pragma once

#include <memory>

#include "manifests/Manifest.hpp"
#include "observability/IValidation.hpp"

namespace chaos::orchestrator::observability {

/**
 * @brief Factory interface for creating validation strategies.
 */
class IValidationFactory {
public:
    virtual ~IValidationFactory() = default;

    /**
     * @brief Creates a validator strategy based on the expectation type.
     * @throws std::invalid_argument If the expectation type is unknown.
     */
    virtual std::unique_ptr<IValidation> create(const manifests::Expectation& expectation) = 0;
};

/**
 * @brief Concrete factory for expectation validators.
 */
class ValidationFactory final : public IValidationFactory {
public:
    ValidationFactory() = default;
    ~ValidationFactory() override = default;

    std::unique_ptr<IValidation> create(const manifests::Expectation& expectation) override;
};

}  // namespace chaos::orchestrator::observability
