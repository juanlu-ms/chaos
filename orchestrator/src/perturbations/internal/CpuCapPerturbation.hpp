/**
 * @file CpuCapPerturbation.hpp
 * @brief Perturbation that restricts a target's CPU usage using cgroups.
 */

#pragma once

#include <atomic>

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that restricts a target's CPU usage using cgroups.
 */
class CpuCapPerturbation final : public IPerturbation {
public:
    /**
     * @brief Construct a CpuCapPerturbation.
     * @param engine Container engine instance.
     * @param target_id Target container ID.
     * @param spec Perturbation specification from manifest.
     */
    explicit CpuCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                const manifests::Perturbation& spec);
    ~CpuCapPerturbation() override = default;

    /**
     * @brief Applies the CPU cap via the Docker Update API.
     * @throws std::system_error On failure to cap CPU.
     */
    void apply() override;

    /**
     * @brief Reverts the CPU cap by restoring the container's default quota.
     * @throws std::system_error On failure to revert CPU cap.
     */
    void revert() override;

private:
    /** @brief The container engine used to interact with the target. */
    std::shared_ptr<containers::IContainerEngine> engine_;

    /** @brief Target ID for the CPU cap perturbation */
    std::string target_id_;

    /** @brief Parameters for the CPU cap perturbation (e.g., quota value). */
    manifests::Parameters params_;

    /** @brief Flag indicating whether the perturbation has been applied. */
    std::atomic<bool> hasBeenApplied_{false};
};

}  // namespace chaos::orchestrator::perturbations
