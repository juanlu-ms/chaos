#pragma once

#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that forcefully stops/kills a target container.
 * Thread-safe execution according to PRD constraints.
 */
class KillPerturbation final : public IPerturbation {
public:
    explicit KillPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id);
    ~KillPerturbation() override = default;

    /**
     * @brief Kills the target container using the provided engine.
     * @throws std::system_error On failure to kill.
     */
    void apply() override;

    /**
     * @brief Reverts the kill by attempting to start the container again.
     * @throws std::system_error On failure to start.
     */
    void revert() override;

private:
    /** @brief The container engine used to interact with the target. */
    std::shared_ptr<containers::IContainerEngine> engine_;

    /** @brief ID of the container to kill */
    std::string target_id_;

    /** @brief Flag indicating whether the perturbation has been applied. */
    bool hasBeenApplied_ = false;
};

}  // namespace chaos::orchestrator::perturbations
