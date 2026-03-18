#pragma once

#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that forcefully stops/kills a target container.
 * Thread-safe execution according to PRD constraints.
 */
class KillPerturbation final : public IPerturbation {
public:
    KillPerturbation() = default;
    ~KillPerturbation() override = default;

    /**
     * @brief Kills the target container using the provided engine.
     * @param engine Reference to the container engine.
     * @param target Manifest target containing the container name.
     * @throws std::system_error On failure to kill.
     */
    void apply(containers::IContainerEngine& engine, const manifests::Target& target) override;
};

}  // namespace chaos::orchestrator::perturbations
