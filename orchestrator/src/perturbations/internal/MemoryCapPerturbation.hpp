/**
 * @file MemoryCapPerturbation.hpp
 * @brief Perturbation that restricts a target's memory using cgroups.
 */

#pragma once

#include <atomic>
#include <memory>

#include "containers/IContainerEngine.hpp"
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
     * @brief Applies the memory cap via the Docker Update API.
     * @throws std::system_error On failure to cap memory.
     */
    void apply() override;

    /**
     * @brief Reverts the memory cap by restoring the system default.
     * @throws std::system_error On failure to revert memory cap.
     */
    void revert() override;

    /**
     * @brief Returns the manifest type name of this perturbation.
     */
    [[nodiscard]] std::string_view type() const override { return "memory_cap"; }

private:
    /** @brief The container engine used to interact with the target. */
    std::shared_ptr<containers::IContainerEngine> engine_;

    /** @brief Target ID for the memory cap perturbation */
    std::string target_id_;

    /** @brief Parameters for the memory cap perturbation, e.g., limit_bytes. */
    manifests::Parameters params_;

    /** @brief Flag indicating whether the perturbation has been applied. */
    std::atomic<bool> hasBeenApplied_{false};

    /** @brief Original memory limit captured before the cap was applied, for safe revert. */
    int64_t original_memory_limit_{0};
};

}  // namespace chaos::orchestrator::perturbations
