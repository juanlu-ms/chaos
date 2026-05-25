/**
 * @file JsonSerializerUnitTest.cpp
 * @brief Unit tests for JSON serialization helpers.
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/ChaosRunner.hpp"
#include "interfaces/web/JsonSerializer.hpp"
#include "observability/ObservabilityEngine.hpp"

using namespace chaos::orchestrator::interfaces::web;

/**
 * @test Verifies stateToJson includes container_id, status, metrics, and phase fields.
 */
TEST(JsonSerializerTest, StateToJsonIncludesPhase) {
    chaos::orchestrator::core::TargetState state;
    state.container_id = "abc123";
    state.status = chaos::orchestrator::containers::ContainerStatus::Running;
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

/**
 * @test Verifies stateToJson omits optional fields when they are not populated.
 */
TEST(JsonSerializerTest, StateToJsonOmitsOptionalFieldsWhenEmpty) {
    chaos::orchestrator::core::TargetState state;
    state.container_id = "abc";
    state.status = chaos::orchestrator::containers::ContainerStatus::Running;

    auto j = stateToJson(state);
    EXPECT_FALSE(j.contains("cpu_usage_percent"));
    EXPECT_FALSE(j.contains("memory_usage_mb"));
    EXPECT_FALSE(j.contains("phase"));
}

/**
 * @test Verifies limitsToJson includes cpu_cores, memory_total_mb, and perturbation_limits.
 */
TEST(JsonSerializerTest, LimitsToJsonContainsCpuAndMemory) {
    chaos::orchestrator::containers::SystemInfo info;
    info.memTotal = 8589934592;

    auto j = limitsToJson(info);
    EXPECT_TRUE(j.contains("cpu_cores"));
    EXPECT_TRUE(j.contains("memory_total_mb"));
    EXPECT_TRUE(j.contains("perturbation_limits"));
}

/**
 * @test Verifies parseLogLines splits input on newline characters.
 */
TEST(JsonSerializerTest, ParseLogLinesSplitsOnNewline) {
    std::vector<std::string> out;
    chaos::orchestrator::observability::parseLogLines("line1\nline2\nline3", out);
    EXPECT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0], "line1");
    EXPECT_EQ(out[1], "line2");
    EXPECT_EQ(out[2], "line3");
}

/**
 * @test Verifies parseLogLines returns an empty list for empty input.
 */
TEST(JsonSerializerTest, ParseLogLinesHandlesEmptyInput) {
    std::vector<std::string> out;
    chaos::orchestrator::observability::parseLogLines("", out);
    EXPECT_TRUE(out.empty());
}

/**
 * @test Verifies parseLogLines strips carriage return characters from lines.
 */
TEST(JsonSerializerTest, ParseLogLinesStripsCarriageReturns) {
    std::vector<std::string> out;
    chaos::orchestrator::observability::parseLogLines("line1\r\nline2\r", out);
    EXPECT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], "line1");
    EXPECT_EQ(out[1], "line2");
}

/**
 * @test Verifies parseLogLines skips empty lines in the input.
 */
TEST(JsonSerializerTest, ParseLogLinesSkipsEmptyLines) {
    std::vector<std::string> out;
    chaos::orchestrator::observability::parseLogLines("line1\n\nline2", out);
    EXPECT_EQ(out.size(), 2u);
}

/**
 * @test Verifies runResultToJson serializes RunResult with pass/fail and results array.
 */
TEST(JsonSerializerTest, SerializeRunResult) {
    chaos::orchestrator::core::RunResult runResult;
    runResult.passed = true;
    runResult.results.push_back(
        chaos::orchestrator::validation::ValidationResult{true, "container_running", "Container is running"});

    auto j = runResultToJson(runResult);

    EXPECT_EQ(j["passed"], true);
    EXPECT_EQ(j["results"].size(), 1);
    EXPECT_EQ(j["results"][0]["type"], "container_running");
    EXPECT_EQ(j["results"][0]["passed"], true);
    EXPECT_EQ(j["results"][0]["message"], "Container is running");
}

/**
 * @test Verifies stateToJson includes network_latency_ms when present.
 */
TEST(JsonSerializerTest, StateToJsonIncludesNetworkLatencyMs) {
    chaos::orchestrator::core::TargetState state;
    state.container_id = "abc";
    state.status = chaos::orchestrator::containers::ContainerStatus::Running;
    state.network_latency_ms = 12.75;

    auto j = stateToJson(state);
    EXPECT_DOUBLE_EQ(j["network_latency_ms"].get<double>(), 12.75);
}

/**
 * @test Verifies stateToJson omits network_latency_ms when not set.
 */
TEST(JsonSerializerTest, StateToJsonOmitsNetworkLatencyMsWhenAbsent) {
    chaos::orchestrator::core::TargetState state;
    state.container_id = "abc";
    state.status = chaos::orchestrator::containers::ContainerStatus::Running;

    auto j = stateToJson(state);
    EXPECT_FALSE(j.contains("network_latency_ms"));
}
