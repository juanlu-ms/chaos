/**
 * @file OtlpRunObserverUnitTest.cpp
 * @brief Unit tests for OtlpRunObserver OTLP run observer.
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "core/RunResult.hpp"
#include "core/TargetState.hpp"
#include "observability/OtlpRunObserver.hpp"

using chaos::orchestrator::observability::OtlpExporter;
using chaos::orchestrator::observability::OtlpRunObserver;
using chaos::orchestrator::observability::resolveOtlpEndpoint;

namespace {

constexpr const char* kOtlpEnvVar = "CHAOS_OTLP_ENDPOINT";

class OtlpRunObserverEnvTest : public ::testing::Test {
protected:
    void SetUp() override { unsetenv(kOtlpEnvVar); }
    void TearDown() override { unsetenv(kOtlpEnvVar); }
};

class OtlpRunObserverTest : public ::testing::Test {
protected:
    void SetUp() override { unsetenv(kOtlpEnvVar); }
    void TearDown() override { unsetenv(kOtlpEnvVar); }

    [[nodiscard]] OtlpExporter unreachableExporter() const { return OtlpExporter("http://127.0.0.1:1"); }
};

/**
 * @test resolveOtlpEndpoint returns nullopt when environment variable is not set.
 */
TEST_F(OtlpRunObserverEnvTest, ResolveReturnsNulloptWhenUnset) {
    auto result = resolveOtlpEndpoint();
    EXPECT_FALSE(result.has_value());
}

/**
 * @test resolveOtlpEndpoint returns the value when environment variable is set.
 */
TEST_F(OtlpRunObserverEnvTest, ResolveReturnsValueWhenSet) {
    setenv(kOtlpEnvVar, "http://otlp.example.com:4318", 1);
    auto result = resolveOtlpEndpoint();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "http://otlp.example.com:4318");
}

/**
 * @test resolveOtlpEndpoint returns nullopt when environment variable is an empty string.
 */
TEST_F(OtlpRunObserverEnvTest, ResolveReturnsNulloptWhenEmpty) {
    setenv(kOtlpEnvVar, "", 1);
    auto result = resolveOtlpEndpoint();
    EXPECT_FALSE(result.has_value());
}

/**
 * @test Constructor completes without throwing.
 */
TEST_F(OtlpRunObserverTest, ConstructorDoesNotThrow) {
    EXPECT_NO_THROW({ OtlpRunObserver observer(unreachableExporter()); });
}

/**
 * @test onPhaseChange does not throw even when the backend is unreachable.
 */
TEST_F(OtlpRunObserverTest, OnPhaseChangeDoesNotThrow) {
    OtlpRunObserver observer(unreachableExporter());
    EXPECT_NO_THROW(observer.onPhaseChange("normal"));
    EXPECT_NO_THROW(observer.onPhaseChange("chaos"));
    EXPECT_NO_THROW(observer.onPhaseChange("recovery"));
}

/**
 * @test onStateUpdate does not throw even when the backend is unreachable.
 */
TEST_F(OtlpRunObserverTest, OnStateUpdateDoesNotThrow) {
    OtlpRunObserver observer(unreachableExporter());
    chaos::orchestrator::core::TargetState state;
    state.container_id = "test-container";
    state.cpu_usage_percent = 42.5;
    state.memory_usage_mb = 128.0;
    state.network_rx_bps = 1000.0;
    state.network_tx_bps = 500.0;

    EXPECT_NO_THROW(observer.onStateUpdate(state));
}

/**
 * @test Rapid onStateUpdate calls within the throttle window do not throw.
 */
TEST_F(OtlpRunObserverTest, OnStateUpdateThrottleDoesNotThrow) {
    OtlpRunObserver observer(unreachableExporter());
    chaos::orchestrator::core::TargetState state;
    state.container_id = "test-container";
    state.cpu_usage_percent = 10.0;
    state.memory_usage_mb = 64.0;

    EXPECT_NO_THROW(observer.onStateUpdate(state));
    EXPECT_NO_THROW(observer.onStateUpdate(state));
    EXPECT_NO_THROW(observer.onStateUpdate(state));
}

/**
 * @test onLogsUpdate and onNetworkLatencyUpdate are no-ops that do not throw.
 */
TEST_F(OtlpRunObserverTest, NoOpMethodsDoNotThrow) {
    OtlpRunObserver observer(unreachableExporter());
    std::vector<std::string> logs{"line1", "line2"};

    EXPECT_NO_THROW(observer.onLogsUpdate(logs));
    EXPECT_NO_THROW(observer.onNetworkLatencyUpdate(1.5));
    EXPECT_NO_THROW(observer.onNetworkLatencyUpdate(std::nullopt));
}

/**
 * @test finalize returns false with an unreachable backend but does not throw.
 */
TEST_F(OtlpRunObserverTest, FinalizeReturnsFalseForUnreachableEndpoint) {
    OtlpRunObserver observer(unreachableExporter());

    chaos::orchestrator::core::RunResult result;
    result.passed = true;
    result.duration_s = 30.0;
    result.manifest_name = "demo-test";
    result.target_id = "abc123";

    bool ok = false;
    EXPECT_NO_THROW({ ok = observer.finalize(result); });
    EXPECT_FALSE(ok);
}

/**
 * @test finalize handles a failed run result without throwing.
 */
TEST_F(OtlpRunObserverTest, FinalizeFailedRunDoesNotThrow) {
    OtlpRunObserver observer(unreachableExporter());

    chaos::orchestrator::core::RunResult result;
    result.passed = false;
    result.duration_s = 15.5;
    result.manifest_name = "failing-test";
    result.target_id = "def456";

    EXPECT_NO_THROW((void)observer.finalize(result));
}

/**
 * @test All observer lifecycle methods through an unreachable backend.
 */
TEST_F(OtlpRunObserverTest, FullObserverLifecycle) {
    OtlpRunObserver observer(unreachableExporter());

    chaos::orchestrator::core::TargetState state;
    state.container_id = "container-1";
    state.cpu_usage_percent = 80.0;
    state.memory_usage_mb = 256.0;
    state.network_rx_bps = 5000.0;
    state.network_tx_bps = 2000.0;

    // Simulate a full run: phase transitions, state updates, network latency.
    EXPECT_NO_THROW(observer.onPhaseChange("normal"));
    EXPECT_NO_THROW(observer.onStateUpdate(state));
    EXPECT_NO_THROW(observer.onPhaseChange("chaos"));
    EXPECT_NO_THROW(observer.onStateUpdate(state));
    EXPECT_NO_THROW(observer.onNetworkLatencyUpdate(2.3));
    EXPECT_NO_THROW(observer.onLogsUpdate({"log line"}));
    EXPECT_NO_THROW(observer.onPhaseChange("recovery"));
    EXPECT_NO_THROW(observer.onStateUpdate(state));

    chaos::orchestrator::core::RunResult result;
    result.passed = true;
    result.duration_s = 60.0;
    result.manifest_name = "full-lifecycle";
    result.target_id = "container-1";

    EXPECT_NO_THROW((void)observer.finalize(result));
}

}  // namespace
