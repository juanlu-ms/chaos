#pragma once

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that restricts a target's CPU usage using cgroups.
 */
class CpuCapPerturbation final : public IPerturbation {
public:
    explicit CpuCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                const manifests::Perturbation& spec);
    ~CpuCapPerturbation() override = default;

    /**
     * @brief Applies the CPU cap by modifying the container's cgroup quotas.
     * @param target Manifest target containing the container name.
     * @throws std::system_error On failure to cap CPU.
     */
    void apply(const manifests::Target& target) override;

    /**
     * @brief Reverts the CPU cap by restoring the container's cgroup quotas.
     * @param target Manifest target containing the container name.
     * @throws std::system_error On failure to revert CPU cap.
     */
    void revert(const manifests::Target& target) override;

private:
    /**
     * @brief The container engine used to interact with the target.
     */
    std::shared_ptr<containers::IContainerEngine> engine_;
    /**
     * @brief Parameters for the CPU cap perturbation (e.g., quota value).
     */
    manifests::Parameters params_;
};

}  // namespace chaos::orchestrator::perturbations
