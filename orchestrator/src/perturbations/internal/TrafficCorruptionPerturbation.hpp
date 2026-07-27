/**
 * @file TrafficCorruptionPerturbation.hpp
 * @brief Perturbation that corrupts, drops, and duplicates container traffic via tc netem.
 */

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Perturbation that applies tc netem rules to corrupt/ drop/ duplicate packets.
 *
 * Applies tc netem rules inside the container's network namespace via nsenter.
 * Supports corruption, packet loss, and duplication via configurable parameters.
 * All rules are reverted cleanly on revert().
 */
class TrafficCorruptionPerturbation final : public IPerturbation {
public:
    /**
     * @brief Construct a TrafficCorruptionPerturbation.
     *
     * Supported parameters (at least one required):
     * - "corrupt_pct" (optional): Packet corruption percentage (e.g. "5%")
     * - "loss_pct" (optional): Packet loss percentage (e.g. "10%")
     * - "duplicate_pct" (optional): Packet duplication percentage (e.g. "3%")
     * - "iface" (optional): Network interface to apply qdisc on (default: "eth0")
     */
    explicit TrafficCorruptionPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                           const manifests::Perturbation& spec);
    ~TrafficCorruptionPerturbation() override = default;

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

    /**
     * @brief Returns the manifest type name of this perturbation.
     */
    [[nodiscard]] std::string_view type() const override { return "traffic_corruption"; }

private:
    std::shared_ptr<containers::IContainerEngine> engine_;
    std::string target_id_;
    manifests::Parameters params_;
    std::atomic<bool> hasBeenApplied_{false};
};

}  // namespace chaos::orchestrator::perturbations
