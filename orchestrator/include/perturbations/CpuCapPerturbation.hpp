#pragma once

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that restricts a target's CPU usage using cgroups.
 */
class CpuCapPerturbation final : public IPerturbation {
public:
    explicit CpuCapPerturbation(manifests::Parameters params);
    ~CpuCapPerturbation() override = default;

    /**
     * @brief Applies the CPU cap by modifying the container's cgroup quotas.
     * @param engine Reference to the container engine.
     * @param target Manifest target containing the container namemanifests.
     * @throws std::system_error On failure to cap CPU.
     */
    void apply(containers::IContainerEngine& engine, const manifests::Target& target) override;

private:
    manifests::Parameters params_;
};

}  // namespace chaos::orchestrator::perturbations
