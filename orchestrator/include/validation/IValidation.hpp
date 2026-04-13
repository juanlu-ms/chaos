/**
 * @file IValidation.hpp
 * @brief Interfaces and context for validation strategies.
 */

#pragma once

#include <string>

#include "ValidationEngine.hpp"
#include "manifests/Manifest.hpp"
#include "observability/ObservabilityEngine.hpp"

namespace chaos::orchestrator::validation {

/**
 * @brief Shared context passed to concrete validation strategies.
 */
struct ValidationContext {
    const chaos::orchestrator::observability::ObservabilityEngine& obs;
    const std::string& containerId;
    std::string& cachedLogs;
    bool& logsFetched;

    /**
     * @brief Lazily fetch and cache logs for the target container.
     * @return Cached logs string.
     */
    const std::string& fetchLogs() const {
        if (!logsFetched) {
            cachedLogs = obs.getLogs(containerId);
            logsFetched = true;
        }
        return cachedLogs;
    }
};

/**
 * @brief Strategy interface for evaluating a single expectation.
 */
class IValidation {
public:
    virtual ~IValidation() = default;

    /**
     * @brief Evaluate the given expectation using the provided context.
     */
    virtual ValidationResult validate(const ValidationContext& ctx,
                                      const manifests::Expectation& expectation) const = 0;
};

}  // namespace chaos::orchestrator::validation
