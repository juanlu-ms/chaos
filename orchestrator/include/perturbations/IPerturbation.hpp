#pragma once

#include <memory>

#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Thread-safe interface for a chaotic perturbation.
 */
class IPerturbation {
public:
    virtual ~IPerturbation() = default;

    /**
     * @brief Apply the perturbation to a specific target.
     *
     * @param engine The container engine used to interact with the target.
     * @param target The target definition from the manifest.
     * @throws std::system_error On failure to apply the perturbation.
     */
    virtual void apply(containers::IContainerEngine& engine, const manifests::Target& target) = 0;
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
     * @param spec The perturbation specification (type and parameters).
     * @return A unique pointer to the instantiated perturbation.
     * @throws std::invalid_argument If the perturbation type is unknown or parameters are invalid.
     */
    virtual std::unique_ptr<IPerturbation> create(const manifests::Perturbation& spec) = 0;
};

}  // namespace chaos::orchestrator::perturbations
