#pragma once

#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that forcefully stops/kills a target container.
 * Thread-safe execution according to PRD constraints.
 */
class KillPerturbation final : public IPerturbation {
public:
    explicit KillPerturbation(std::shared_ptr<containers::IContainerEngine> engine);
    ~KillPerturbation() override = default;

    /**
     * @brief Kills the target container using the provided engine.
     * @param engine Reference to the container engine.
     * @param target Manifest target containing the container name.
     * @throws std::system_error On failure to kill.
     */
    void apply(const manifests::Target& target) override;

    /**
     * @brief No revert action for kill perturbation.
     * @param target Manifest target containing the container name.
     * TODO: Change throws.
     * @throws error If called.
     */
    void revert(const manifests::Target& target) override;

private:
    /**
     * @brief The container engine used to interact with the target.
     */
    std::shared_ptr<containers::IContainerEngine> engine_;
};

}  // namespace chaos::orchestrator::perturbations
