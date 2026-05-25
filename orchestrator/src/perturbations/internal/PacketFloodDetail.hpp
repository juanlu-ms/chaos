/**
 * @file PacketFloodDetail.hpp
 * @brief Testable helpers for checksum computation and network namespace manipulation.
 */

#pragma once

#include <cstdint>
#include <vector>

namespace chaos::orchestrator::perturbations::detail {

// RAII guard that opens and saves the current network namespace on construction,
// and restores it via setns() on destruction.
class ScopedNamespaceGuard {
public:
    // Open the network namespace of the process identified by pid.
    // Throws std::system_error on failure.
    explicit ScopedNamespaceGuard(int pid);
    ~ScopedNamespaceGuard();

    ScopedNamespaceGuard(const ScopedNamespaceGuard&) = delete;
    ScopedNamespaceGuard& operator=(const ScopedNamespaceGuard&) = delete;

private:
    int originalNsFd_ = -1;
};

// Compute the TCP pseudo-header checksum (IP addresses + protocol + length).
// Returns 32-bit sum for folding into the final checksum.
[[nodiscard]] uint32_t pseudoHeaderChecksum(uint32_t srcIp, uint32_t dstIp, uint16_t tcpLen);

// Compute the final TCP checksum from segment data and pseudo-header sum.
// Returns 16-bit one's complement checksum in network byte order.
[[nodiscard]] uint16_t segmentChecksum(const std::vector<uint8_t>& data, uint32_t pseudoSum);

}  // namespace chaos::orchestrator::perturbations::detail
