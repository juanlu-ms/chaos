#pragma once

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that restricts a target's memory using cgroups.
 */
class MemoryCapPerturbation final : public IPerturbation {
public:
    explicit MemoryCapPerturbation(manifests::Parameters params);
    ~MemoryCapPerturbation() override = default;

    /**
     * @brief Applies the memory cap by modifying the container's cgroup.
     * @param engine Reference to the container engine.
     * @param target Manifest target containing the container name.
     * @throws std::system_error On failure to cap memory.
     */
    void apply(containers::IContainerEngine& engine, const manifests::Target& target) override;

private:
    manifests::Parameters params_;
};

}  // namespace chaos::orchestrator::perturbations
