/**
 * @file PacketFloodPerturbation.hpp
 * @brief Perturbation that floods a container with crafted IP packets (DDoS-like).
 */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>

#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief DDoS-like perturbation that floods the target container with crafted IP packets.
 *
 * Enters the container's network namespace via setns(), opens a raw AF_PACKET socket,
 * then floods the container's own interface with structurally valid Ethernet+IP frames
 * targeted at the container's own IP address. Payloads are random garbage with spoofed
 * source addresses, causing resource exhaustion rather than a traffic cutoff.
 *
 * After revert() the flood stops and the container returns to normal operation.
 */
class PacketFloodPerturbation final : public IPerturbation {
public:
    /**
     * @brief Construct a PacketFloodPerturbation.
     *
     * Supported parameters (all optional):
     * - "iface" (optional): Network interface to flood (default: "eth0")
     * - "rate" (optional): Packets per second (default: "1000")
     * - "packet_size" (optional): Frame size in bytes (default: "128")
     */
    explicit PacketFloodPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                     const manifests::Perturbation& spec);
    ~PacketFloodPerturbation() override = default;

    PacketFloodPerturbation(const PacketFloodPerturbation&) = delete;
    PacketFloodPerturbation& operator=(const PacketFloodPerturbation&) = delete;
    PacketFloodPerturbation(PacketFloodPerturbation&&) = delete;
    PacketFloodPerturbation& operator=(PacketFloodPerturbation&&) = delete;

    /**
     * @brief Opens a raw socket in the container's netns and starts the flood thread.
     * @throws std::invalid_argument If target ID is empty.
     * @throws std::system_error On failure to open raw socket or enter netns.
     */
    void apply() override;

    /**
     * @brief Stops the flood thread and closes the raw socket.
     */
    void revert() override;

    /**
     * @brief Returns the manifest type name of this perturbation.
     */
    [[nodiscard]] std::string_view type() const override { return "packet_flood"; }

private:
    void setupSocketInNetns();
    void floodLoop(const std::stop_token& stop) const;

    std::shared_ptr<containers::IContainerEngine> engine_;
    std::string target_id_;
    std::string iface_;
    std::string containerIp_;
    std::array<uint8_t, 6> localMac_{};
    int rate_ = 1000;
    int packet_size_ = 128;

    int raw_sd_ = -1;
    int ifindex_ = 0;
    std::jthread flood_thread_;
    std::atomic<bool> hasBeenApplied_{false};
};

}  // namespace chaos::orchestrator::perturbations
