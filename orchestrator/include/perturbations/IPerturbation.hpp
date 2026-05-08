/**
 * @file IPerturbation.hpp
 * @brief Interfaces for defining and creating perturbations.
 */

#pragma once

#include <memory>

#include "../containers/IContainerEngine.hpp"
#include "../manifests/Manifest.hpp"

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

/**
 * @brief Factory for instantiating different types of perturbations.
 */
class IPerturbationFactory {
public:
    virtual ~IPerturbationFactory() = default;

    /**
     * @brief Creates a perturbation based on the manifest specification.
     *
     * @param engine The container engine used to interact with the target.
     * @param target The target specification.
     * @param spec The perturbation specification (type and parameters).
     * @return A unique pointer to the instantiated perturbation.
     * @throws std::invalid_argument If the perturbation type is unknown or parameters are invalid.
     */
    virtual std::unique_ptr<IPerturbation> create(std::shared_ptr<containers::IContainerEngine> engine,
                                                  const manifests::Target& target,
                                                  const manifests::Perturbation& spec) = 0;
};

}  // namespace chaos::orchestrator::perturbations
