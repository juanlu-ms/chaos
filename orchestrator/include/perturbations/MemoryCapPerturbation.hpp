/**
 * @file MemoryCapPerturbation.hpp
 * @brief Perturbation that restricts a target's memory using cgroups.
 */

#pragma once

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that restricts a target's memory using cgroups.
 */
class MemoryCapPerturbation final : public IPerturbation {
public:
    /**
     * @brief Construct a MemoryCapPerturbation.
     * @param engine Container engine instance.
     * @param target_id Target container ID.
     * @param spec Perturbation specification from manifest.
     */
    explicit MemoryCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                   const manifests::Perturbation& spec);
    ~MemoryCapPerturbation() override = default;

    /**
     * @brief Applies the memory cap by modifying the container's cgroup.
     * @throws std::system_error On failure to cap memory.
     */
    void apply() override;

    /**
     * @brief Reverts the memory cap by removing the cgroup restriction.
     * @throws std::system_error On failure to revert memory cap.
     */
    void revert() override;

private:
    /** @brief The container engine used to interact with the target. */
    std::shared_ptr<containers::IContainerEngine> engine_;

    /** @brief Target ID for the memory cap perturbation */
    std::string target_id_;

    /** @brief Parameters for the memory cap perturbation, e.g., limit_bytes. */
    manifests::Parameters params_;

    /** @brief Flag indicating whether the perturbation has been applied. */
    bool hasBeenApplied_ = false;
};

}  // namespace chaos::orchestrator::perturbations
