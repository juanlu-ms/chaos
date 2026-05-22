/**
 * @file JsonSerializerUnitTest.cpp
 * @brief Unit tests for JSON serialization helpers.
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "observability/ObservabilityEngine.hpp"
#include "web/JsonSerializer.hpp"

using namespace chaos::orchestrator::interfaces::web;

TEST(JsonSerializerTest, StateToJsonIncludesPhase) {
    chaos::orchestrator::shared::TargetState state;
    state.container_id = "abc123";
    state.status = chaos::orchestrator::shared::ContainerStatus::Running;
    state.cpu_usage_percent = 42.5;
    state.memory_usage_mb = 128.0;
    state.network_rx_bps = 1000.0;
    state.network_tx_bps = 2000.0;

    auto j = stateToJson(state, "chaos");
    EXPECT_EQ(j["container_id"], "abc123");
    EXPECT_EQ(j["status"], "running");
    EXPECT_DOUBLE_EQ(j["cpu_usage_percent"].get<double>(), 42.5);
    EXPECT_DOUBLE_EQ(j["memory_usage_mb"].get<double>(), 128.0);
    EXPECT_DOUBLE_EQ(j["network_rx_bps"].get<double>(), 1000.0);
    EXPECT_DOUBLE_EQ(j["network_tx_bps"].get<double>(), 2000.0);
    EXPECT_EQ(j["phase"], "chaos");
}

TEST(JsonSerializerTest, StateToJsonOmitsOptionalFieldsWhenEmpty) {
    chaos::orchestrator::shared::TargetState state;
    state.container_id = "abc";
    state.status = chaos::orchestrator::shared::ContainerStatus::Running;

    auto j = stateToJson(state);
    EXPECT_FALSE(j.contains("cpu_usage_percent"));
    EXPECT_FALSE(j.contains("memory_usage_mb"));
    EXPECT_FALSE(j.contains("phase"));
}

TEST(JsonSerializerTest, LimitsToJsonContainsCpuAndMemory) {
    chaos::orchestrator::containers::SystemInfo info;
    info.memTotal = 8589934592;

    auto j = limitsToJson(info);
    EXPECT_TRUE(j.contains("cpu_cores"));
    EXPECT_TRUE(j.contains("memory_total_mb"));
    EXPECT_TRUE(j.contains("perturbation_limits"));
}

TEST(JsonSerializerTest, ParseLogLinesSplitsOnNewline) {
    std::vector<std::string> out;
    chaos::orchestrator::observability::parseLogLines("line1\nline2\nline3", out);
    EXPECT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0], "line1");
    EXPECT_EQ(out[1], "line2");
    EXPECT_EQ(out[2], "line3");
}

TEST(JsonSerializerTest, ParseLogLinesHandlesEmptyInput) {
    std::vector<std::string> out;
    chaos::orchestrator::observability::parseLogLines("", out);
    EXPECT_TRUE(out.empty());
}

TEST(JsonSerializerTest, ParseLogLinesStripsCarriageReturns) {
    std::vector<std::string> out;
    chaos::orchestrator::observability::parseLogLines("line1\r\nline2\r", out);
    EXPECT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], "line1");
    EXPECT_EQ(out[1], "line2");
}

TEST(JsonSerializerTest, ParseLogLinesSkipsEmptyLines) {
    std::vector<std::string> out;
    chaos::orchestrator::observability::parseLogLines("line1\n\nline2", out);
    EXPECT_EQ(out.size(), 2u);
}
