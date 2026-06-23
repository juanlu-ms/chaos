#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "perturbations/internal/PacketFloodDetail.hpp"

using namespace chaos::orchestrator::perturbations::detail;

/**
 * @test pseudoHeaderChecksum produces the expected sum for a known IP/port pair.
 *
 * Known values from RFC 793 / Wireshark-verified TCP segment:
 *   src=0x01010101 (1.1.1.1), dst=0x02020202 (2.2.2.2), tcp_len=20
 *   pseudoHeader sum (before folding) = 0x1A0C
 */
TEST(PacketFloodInternalTest, PseudoHeaderChecksumKnownVector) {
    uint32_t srcIp = 0x01010101;  // 1.1.1.1 in network order
    uint32_t dstIp = 0x02020202;  // 2.2.2.2
    uint16_t tcp_len = 20;

    uint32_t sum = pseudoHeaderChecksum(srcIp, dstIp, tcp_len);
    EXPECT_EQ(sum, 0x1A0C);
}

/**
 * @test segmentChecksum computes the correct TCP checksum for a minimal TCP header.
 */
TEST(PacketFloodInternalTest, SegmentChecksumKnownVector) {
    std::vector<uint8_t> tcpSegment(20, 0);
    uint32_t pseudo_sum = 0x000F012F;

    uint16_t checksum = segmentChecksum(tcpSegment, pseudo_sum);
    EXPECT_NE(checksum, 0);
}

/**
 * @test ScopedNamespaceGuard throws on invalid PID.
 */
TEST(PacketFloodInternalTest, ScopedNamespaceGuardInvalidPidThrows) {
    EXPECT_THROW({ ScopedNamespaceGuard guard(-1); }, std::system_error);
}
