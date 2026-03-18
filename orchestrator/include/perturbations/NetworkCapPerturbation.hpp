#pragma once

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that restricts a target's network interface using tc.
 */
class NetworkCapPerturbation final : public IPerturbation {
public:
    explicit NetworkCapPerturbation(manifests::Parameters params);
    ~NetworkCapPerturbation() override = default;

    /**
     * @brief Applies network restrictions (latency/bandwidth) using Linux tc inside the container's network namespace.
     * @param engine Reference to the container engine.
     * @param target Manifest target containing the container namemanifests.
     * @throws std::system_error On failure to cap network.
     */
    void apply(containers::IContainerEngine& engine, const manifests::Target& target) override;

private:
    manifests::Parameters params_;
};

}  // namespace chaos::orchestrator::perturbations
