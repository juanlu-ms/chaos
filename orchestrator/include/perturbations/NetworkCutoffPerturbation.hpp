#pragma once

#include <string>
#include <vector>

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that drops network traffic for a target container.
 *
 * Applies iptables rules inside the container's network namespace via nsenter.
 * Rules can selectively drop traffic from specific IPs or ports, or all traffic.
 * All rules are reverted cleanly on revert(), preserving reconnectability.
 */
class NetworkCutoffPerturbation final : public IPerturbation {
public:
    /**
     * @brief Construct a NetworkCutoffPerturbation.
     *
     * Supported parameters:
     * - "dst_ip" (optional): Destination IP to block (e.g. "8.8.8.8")
     * - "dst_port" (optional): Destination port to block (e.g. "443")
     * - "src_port" (optional): Source port to block incoming traffic from
     * - If no filters provided, all traffic is blocked (INPUT and OUTPUT DROP policy)
     */
    explicit NetworkCutoffPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                       const manifests::Perturbation& spec);
    ~NetworkCutoffPerturbation() override = default;

    /**
     * @brief Applies iptables DROP rules inside the container's network namespace.
     * @throws std::invalid_argument If target ID is empty.
     * @throws std::system_error On failure to apply iptables rule.
     */
    void apply() override;

    /**
     * @brief Reverts iptables DROP rules inside the container's network namespace.
     * @throws std::system_error On failure to remove iptables rule.
     */
    void revert() override;

private:
    /** @brief The container engine used to interact with the target. */
    std::shared_ptr<containers::IContainerEngine> engine_;

    /** @brief Target ID for the network cutoff perturbation. */
    std::string target_id_;

    /** @brief Parameters for the network cutoff (e.g., dst_ip, dst_port, src_port). */
    manifests::Parameters params_;

    /** @brief Applied iptables rules to revert (stored as -D equivalents). */
    std::vector<std::string> revertCommands_;

    /** @brief Flag indicating whether the perturbation has been applied. */
    bool hasBeenApplied_ = false;
};

}  // namespace chaos::orchestrator::perturbations
