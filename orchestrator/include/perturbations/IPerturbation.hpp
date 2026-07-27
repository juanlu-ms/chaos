/**
 * @file IPerturbation.hpp
 * @brief Interfaces for defining and creating perturbations.
 */

#pragma once

#include <string_view>

namespace chaos::orchestrator::perturbations {

/**
 * @brief Thread-safe interface for a chaotic perturbation.
 */
class IPerturbation {
public:
    virtual ~IPerturbation() = default;

    /**
     * @brief Apply the perturbation to the target bound at construction time.
     *
     * @throws std::system_error On failure to apply the perturbation.
     */
    virtual void apply() = 0;

    /**
     * @brief Revert the perturbation from the target bound at construction time.
     *
     * @throws std::system_error On failure to revert the perturbation.
     */
    virtual void revert() = 0;

    /**
     * @brief Get the manifest type name of this perturbation, e.g. "network_delay".
     *
     * Matches the spec type accepted by PerturbationFactory::create, so failures can
     * be reported back to the user in the same vocabulary as the manifest.
     *
     * @return Type name, valid for the lifetime of the perturbation.
     */
    [[nodiscard]] virtual std::string_view type() const = 0;
};

}  // namespace chaos::orchestrator::perturbations
