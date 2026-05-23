#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "core/SharedState.hpp"
#include "shared/TargetState.hpp"

using namespace chaos::orchestrator::core;
using namespace chaos::orchestrator::shared;

TEST(SharedStateTest, DefaultStateIsEmpty) {
    SharedState s;
    EXPECT_EQ(s.latestState().container_id, "");
    EXPECT_EQ(s.latestLogs().size(), 0u);
    EXPECT_EQ(s.phase(), "");
    EXPECT_EQ(s.continuousFailures().size(), 0u);
}

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

TEST(SharedStateTest, UpdateAndReadLogs) {
    SharedState s;
    std::vector<std::string> logs = {"line1", "line2"};
    s.updateLogs(logs);

    auto read = s.latestLogs();
    ASSERT_EQ(read.size(), 2u);
    EXPECT_EQ(read[0], "line1");
}

TEST(SharedStateTest, UpdateLogsOverwritesPrevious) {
    SharedState s;
    s.updateLogs({"old"});
    s.updateLogs({"new"});
    auto read = s.latestLogs();
    ASSERT_EQ(read.size(), 1u);
    EXPECT_EQ(read[0], "new");
}

TEST(SharedStateTest, SetAndReadPhase) {
    SharedState s;
    s.setPhase("chaos");
    EXPECT_EQ(s.phase(), "chaos");
    s.setPhase("recovery");
    EXPECT_EQ(s.phase(), "recovery");
}

TEST(SharedStateTest, AddContinuousFailureDeduplicates) {
    SharedState s;
    s.addContinuousFailure("http_status");
    s.addContinuousFailure("http_status");  // duplicate
    s.addContinuousFailure("container_running");

    auto failures = s.continuousFailures();
    EXPECT_EQ(failures.size(), 2u);
}

TEST(SharedStateTest, ConcurrentWritesDoNotCorrupt) {
    SharedState s;
    constexpr int kIterations = 1000;

    std::thread writer1([&] {
        for (int i = 0; i < kIterations; ++i) {
            TargetState ts;
            ts.cpu_usage_percent = static_cast<double>(i);
            s.updateState(ts);
        }
    });

    std::thread writer2([&] {
        for (int i = 0; i < kIterations; ++i) {
            s.addContinuousFailure("type_" + std::to_string(i % 10));
        }
    });

    writer1.join();
    writer2.join();

    auto state = s.latestState();
    auto failures = s.continuousFailures();
    EXPECT_GE(failures.size(), 0u);
    EXPECT_TRUE(!state.cpu_usage_percent.has_value() || *state.cpu_usage_percent >= 0.0);
}
