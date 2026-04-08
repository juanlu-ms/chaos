/**
 * @file PerturbationFactory.hpp
 * @brief Concrete factory class to instantiate IPerturbation objects.
 */

#pragma once

#include <memory>

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Factory class to instantiate IPerturbation objects.
 */
class PerturbationFactory final : public IPerturbationFactory {
public:
    PerturbationFactory() = default;
    ~PerturbationFactory() override = default;

    /**
     * @brief Creates the concrete perturbation based on the spec type.
     * @param engine The container engine used to interact with the target.
     * @param target The target specification.
     * @param spec The parsed perturbation definition (e.g., type="MemoryCap").
     * @return Unique pointer to the instantiated IPerturbation.
     * @throws std::invalid_argument If the type is not recognized.
     */
    std::unique_ptr<IPerturbation> create(std::shared_ptr<containers::IContainerEngine> engine,
                                          const manifests::Target& target,
                                          const manifests::Perturbation& spec) override;
};

}  // namespace chaos::orchestrator::perturbations
