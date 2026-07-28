/**
 * @file MemoryCapPerturbation.hpp
 * @brief Perturbation that restricts a target's memory using cgroups.
 */

#pragma once

#include <atomic>
#include <chrono>
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
     *
     * A cap set below the target's current usage kills it outright, which is the intended effect of
     * this perturbation. When that happens the engine may still report the update as failed, so a
     * target that was OOM-killed — or that is otherwise no longer running — counts as a successful
     * application rather than an error.
     *
     * @throws std::system_error On failure to cap memory while the target is still alive.
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
    /**
     * @brief Decides whether a failed update still delivered the fault, by inspecting the target.
     *
     * Prefers direct evidence that the kernel OOM-killed the target; falls back to the target no
     * longer running. A target still running is re-checked a few times, since the engine may take a
     * moment to reflect its death.
     *
     * @return True if the cap should be considered applied despite the update error.
     */
    [[nodiscard]] bool targetDiedApplyingCap() const;

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

    /** @brief Times the target's state is re-checked after a failed update before giving up. */
    static constexpr int kStatusCheckAttempts = 3;

    /** @brief Delay between state re-checks, giving the engine time to reflect the target's death. */
    static constexpr std::chrono::milliseconds kStatusCheckDelay{100};
};

}  // namespace chaos::orchestrator::perturbations
