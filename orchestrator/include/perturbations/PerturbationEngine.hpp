/**
 * @file PerturbationEngine.hpp
 * @brief Engine for encapsulating perturbation lifecycle management.
 */

#pragma once

#include <memory>
#include <vector>

#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Manages the safe lifecycle of perturbations.
 * Creates, applies, and guarantees reversion of perturbations.
 */
class PerturbationEngine {
public:
    /**
     * @brief Construct a new PerturbationEngine.
     * @param engine Container engine instance to wrap.
     */
    explicit PerturbationEngine(std::shared_ptr<containers::IContainerEngine> engine);

    /**
     * @brief Reverts any active perturbations (RAII).
     */
    ~PerturbationEngine();

    // Delete copy construction and assignment
    PerturbationEngine(const PerturbationEngine&) = delete;
    PerturbationEngine& operator=(const PerturbationEngine&) = delete;

    /**
     * @brief Instantiates and applies all perturbations defined in a manifest.
     *
     * @param manifest The chaos manifestation containing the target and perturbation specs.
     * @throws std::exception on failure to instantiate or apply perturbations.
     */
    void applyAll(const manifests::ChaosManifest& manifest);

    /**
     * @brief Reverts all active perturbations.
     * Reverts are executed in reverse order of application.
     */
    void revertAll();

private:
    std::shared_ptr<containers::IContainerEngine> engine_;
    std::vector<std::unique_ptr<IPerturbation>> active_perturbations_;
};

}  // namespace chaos::orchestrator::perturbations
