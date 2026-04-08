/**
 * @file GarbagePacketPerturbation.hpp
 * @brief Perturbation that injects garbage packets into container traffic.
 */

#pragma once

#include <string>

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Concrete perturbation that injects garbage packets into container traffic.
 *
 * Applies tc netem rules inside the container's network namespace via nsenter.
 * Supports corruption, packet loss, and duplication via configurable parameters.
 * All rules are reverted cleanly on revert().
 */
class GarbagePacketPerturbation final : public IPerturbation {
public:
    /**
     * @brief Construct a GarbagePacketPerturbation.
     *
     * Supported parameters (at least one required):
     * - "corrupt_pct" (optional): Packet corruption percentage (e.g. "5%")
     * - "loss_pct" (optional): Packet loss percentage (e.g. "10%")
     * - "duplicate_pct" (optional): Packet duplication percentage (e.g. "3%")
     * - "iface" (optional): Network interface to apply qdisc on (default: "eth0")
     */
    explicit GarbagePacketPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                       const manifests::Perturbation& spec);
    ~GarbagePacketPerturbation() override = default;

    /**
     * @brief Applies tc netem rules inside the container's network namespace.
     * @throws std::invalid_argument If target ID is empty or no parameters provided.
     * @throws std::system_error On failure to apply tc netem rule.
     */
    void apply() override;

    /**
     * @brief Reverts tc netem rules inside the container's network namespace.
     * @throws std::system_error On failure to remove tc netem rule.
     */
    void revert() override;

private:
    /** @brief The container engine used to interact with the target. */
    std::shared_ptr<containers::IContainerEngine> engine_;

    /** @brief Target ID for the garbage packet perturbation. */
    std::string target_id_;

    /** @brief Parameters for garbage packet injection. */
    manifests::Parameters params_;

    /** @brief Flag indicating whether the perturbation has been applied. */
    bool hasBeenApplied_ = false;
};

}  // namespace chaos::orchestrator::perturbations
