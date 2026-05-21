/**
 * @file NetworkDelayPerturbation.hpp
 * @brief Perturbation that restricts a target's network interface using tc.
 */

#pragma once

#include <atomic>
#include <memory>

#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that restricts a target's network interface using tc.
 */
class NetworkDelayPerturbation final : public IPerturbation {
public:
    /**
     * @brief Construct a NetworkDelayPerturbation.
     * @param engine Container engine instance.
     * @param target_id Target container ID.
     * @param spec Perturbation specification from manifest.
     */
    explicit NetworkDelayPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                      const manifests::Perturbation& spec);
    ~NetworkDelayPerturbation() override = default;

    /**
     * @brief Applies network restrictions (latency/bandwidth) using Linux tc inside the container's network namespace.
     * @throws std::system_error On failure to cap network.
     */
    void apply() override;

    /**
     * @brief Reverts network restrictions by removing tc rules.
     * @throws std::system_error On failure to revert network cap.
     */
    void revert() override;

private:
    /** @brief The container engine used to interact with the target. */
    std::shared_ptr<containers::IContainerEngine> engine_;

    /** @brief Target ID for the network cap perturbation */
    std::string target_id_;

    /** @brief Parameters for the network cap perturbation, e.g., delay_ms, bandwidth_limit. */
    manifests::Parameters params_;

    /** @brief Flag indicating whether the perturbation has been applied. */
    std::atomic<bool> hasBeenApplied_{false};
};

}  // namespace chaos::orchestrator::perturbations
