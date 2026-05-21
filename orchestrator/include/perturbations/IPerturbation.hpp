/**
 * @file IPerturbation.hpp
 * @brief Interfaces for defining and creating perturbations.
 */

#pragma once

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
};

}  // namespace chaos::orchestrator::perturbations
