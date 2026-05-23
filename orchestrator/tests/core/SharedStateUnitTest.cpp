/**
 * @file SharedStateUnitTest.cpp
 * @brief Unit tests for SharedState thread-safe data bucket.
 */

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "core/SharedState.hpp"
#include "shared/TargetState.hpp"

using namespace chaos::orchestrator::core;
using namespace chaos::orchestrator::shared;

/**
 * @test Verifies that a default-constructed SharedState has empty/zero values.
 */
TEST(SharedStateTest, DefaultStateIsEmpty) {
    SharedState s;
    EXPECT_EQ(s.latestState().container_id, "");
    EXPECT_EQ(s.latestLogs().size(), 0u);
    EXPECT_EQ(s.phase(), "");
    EXPECT_EQ(s.continuousFailures().size(), 0u);
}

/**
 * @test Verifies that updateState/latestState round-trips correctly.
 */
TEST(SharedStateTest, UpdateAndReadState) {
    SharedState s;
    TargetState ts;
    ts.container_id = "abc123";
    ts.cpu_usage_percent = 42.0;
    s.updateState(ts);

    auto read = s.latestState();
    EXPECT_EQ(read.container_id, "abc123");
    ASSERT_TRUE(read.cpu_usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*read.cpu_usage_percent, 42.0);
}

/**
 * @test Verifies that updateLogs/latestLogs round-trips correctly.
 */
TEST(SharedStateTest, UpdateAndReadLogs) {
    SharedState s;
    std::vector<std::string> logs = {"line1", "line2"};
    s.updateLogs(logs);

    auto read = s.latestLogs();
    ASSERT_EQ(read.size(), 2u);
    EXPECT_EQ(read[0], "line1");
    EXPECT_EQ(read[1], "line2");
}

/**
 * @test Verifies that updateLogs overwrites previously stored logs.
 */
TEST(SharedStateTest, UpdateLogsOverwritesPrevious) {
    SharedState s;
    s.updateLogs({"old"});
    s.updateLogs({"new"});
    auto read = s.latestLogs();
    ASSERT_EQ(read.size(), 1u);
    EXPECT_EQ(read[0], "new");
}

/**
 * @test Verifies setPhase/phase round-trips correctly.
 */
TEST(SharedStateTest, SetAndReadPhase) {
    SharedState s;
    s.setPhase("chaos");
    EXPECT_EQ(s.phase(), "chaos");
    s.setPhase("recovery");
    EXPECT_EQ(s.phase(), "recovery");
}

/**
 * @test Verifies that duplicate continuous failures are deduplicated.
 */
TEST(SharedStateTest, AddContinuousFailureDeduplicates) {
    SharedState s;
    s.addContinuousFailure("http_status");
    s.addContinuousFailure("http_status");  // duplicate
    s.addContinuousFailure("container_running");

    auto failures = s.continuousFailures();
    EXPECT_EQ(failures.size(), 2u);
}

/**
 * @test Verifies that concurrent writes to different fields do not corrupt state.
 */
TEST(SharedStateTest, ConcurrentWritesDoNotCorrupt) {
    SharedState s;
    constexpr int kIterations = 1000;

    std::thread writer1([&s] {
        for (int i = 0; i < kIterations; ++i) {
            TargetState ts;
            ts.cpu_usage_percent = static_cast<double>(i);
            s.updateState(ts);
        }
    });

    std::thread writer2([&s] {
        for (int i = 0; i < kIterations; ++i) {
            s.addContinuousFailure(fmt::format("type_{}", i % 10));
        }
    });

    writer1.join();
    writer2.join();

    auto state = s.latestState();
    auto failures = s.continuousFailures();
    EXPECT_EQ(failures.size(), 10u);
    EXPECT_TRUE(!state.cpu_usage_percent.has_value() || *state.cpu_usage_percent >= 0.0);
}
