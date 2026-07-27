/**
 * @file KillPerturbation.hpp
 * @brief Perturbation that forcefully stops/kills a target container.
 */

#pragma once

#include <atomic>
#include <memory>

#include "containers/IContainerEngine.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that forcefully stops/kills a target container.
 * Thread-safe execution according to PRD constraints.
 */
class KillPerturbation final : public IPerturbation {
public:
    /**
     * @brief Construct a KillPerturbation.
     * @param engine Container engine instance.
     * @param target_id Target container ID.
     */
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

    /**
     * @brief Returns the manifest type name of this perturbation.
     */
    [[nodiscard]] std::string_view type() const override { return "kill"; }

private:
    /** @brief The container engine used to interact with the target. */
    std::shared_ptr<containers::IContainerEngine> engine_;

    /** @brief ID of the container to kill */
    std::string target_id_;

    /** @brief Flag indicating whether the perturbation has been applied. */
    std::atomic<bool> hasBeenApplied_{false};
};

}  // namespace chaos::orchestrator::perturbations
