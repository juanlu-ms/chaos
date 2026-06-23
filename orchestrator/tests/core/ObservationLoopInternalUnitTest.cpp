/**
 * @file ObservationLoopInternalUnitTest.cpp
 * @brief Unit tests for ObservationLoop helpers.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <fstream>

#include "core/internal/ObservationLoopDetail.hpp"

using namespace chaos::orchestrator::core::detail;
using namespace std::chrono_literals;

/**
 * @test parseProcNetDev returns zeros on first call (no previous snapshot).
 */
TEST(ObservationLoopInternalTest, ParseProcNetDevFirstCallReturnsZeros) {
    // Read the current process's /proc/net/dev to get a valid file
    // (this test checks the initial-call behavior, not the actual values)
    std::ifstream ifs("/proc/self/net/dev");
    ASSERT_TRUE(ifs.good()) << "This test requires /proc to be available";

    uint64_t prev_rx = 0, prev_tx = 0;
    auto prev_time = std::chrono::steady_clock::time_point{};
    bool prev_valid = false;

    auto result = parseProcNetDev(getpid(), prev_rx, prev_tx, prev_time, prev_valid);

    // On first call with no valid previous state, rates should be 0
    EXPECT_EQ(result.rx_bps, 0.0);
    EXPECT_EQ(result.tx_bps, 0.0);
    EXPECT_TRUE(prev_valid);
}

/**
 * @test parseProcNetDev with valid previous snapshot computes rate deltas on second call.
 */
TEST(ObservationLoopInternalTest, ParseProcNetDevSecondCallWithValidPrev) {
    uint64_t prev_rx = 0, prev_tx = 0;
    auto prev_time = std::chrono::steady_clock::time_point{};
    bool prev_valid = false;

    auto first = parseProcNetDev(getpid(), prev_rx, prev_tx, prev_time, prev_valid);
    EXPECT_EQ(first.rx_bps, 0.0);
    EXPECT_EQ(first.tx_bps, 0.0);
    ASSERT_TRUE(prev_valid);

    auto second = parseProcNetDev(getpid(), prev_rx, prev_tx, prev_time, prev_valid);
    EXPECT_TRUE(prev_valid);
    EXPECT_GE(second.rx_bps, 0.0);
    EXPECT_GE(second.tx_bps, 0.0);
}

/**
 * @test executePing returns a positive RTT against localhost.
 */
TEST(ObservationLoopInternalTest, ExecutePingLocalhostReturnsPositiveRtt) {
    auto rtt = executePing("127.0.0.1");
    EXPECT_TRUE(rtt.has_value());
    if (rtt) {
        EXPECT_GT(*rtt, 0.0);
    }
}

/**
 * @test executePing returns nullopt for an unreachable address.
 */
TEST(ObservationLoopInternalTest, ExecutePingUnreachableReturnsNullopt) {
    auto rtt = executePing("192.0.2.1");
    EXPECT_FALSE(rtt.has_value());
}

/**
 * @test interruptibleSleep returns early when stop is requested.
 */
TEST(ObservationLoopInternalTest, InterruptibleSleepStopsEarlyOnCancellation) {
    std::stop_source src;
    auto start = std::chrono::steady_clock::now();

    std::thread t([&] {
        std::this_thread::sleep_for(10ms);
        src.request_stop();
    });

    interruptibleSleep(5s, src.get_token());
    auto elapsed = std::chrono::steady_clock::now() - start;
    t.join();

    EXPECT_LT(elapsed, 1s);
}
