/// @file ObservabilityEngineUnitTest.cpp
/// @brief Unit tests for ObservabilityEngine.

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

#include "MockContainerEngine.hpp"
#include "containers/Container.hpp"
#include "observability/ObservabilityEngine.hpp"

using namespace testing;
using namespace chaos::orchestrator;

namespace {

constexpr auto kContainerId = "abc123";

}  // namespace

/**
 * @test Verifies observe returns full metrics when target is running.
 */
TEST(ObservabilityEngineTests, ObserveReturnsFullStateWhenRunning) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, getStatus(std::string_view(kContainerId)))
        .WillOnce(Return(shared::ContainerStatus::Running));
    EXPECT_CALL(*mockEngine, getLogs(std::string_view(kContainerId))).WillOnce(Return("log line"));
    EXPECT_CALL(*mockEngine, getContainerMemoryUsage(std::string_view(kContainerId))).WillOnce(Return(128.5));
    EXPECT_CALL(*mockEngine, getContainerCpuUsage(std::string_view(kContainerId))).WillOnce(Return(12.25));
    EXPECT_CALL(*mockEngine, getContainerIp(std::string_view(kContainerId))).WillOnce(Return("10.0.0.5"));

    observability::ObservabilityEngine obs(mockEngine);
    const auto state = obs.observe(kContainerId);

    EXPECT_EQ(state.container_id, kContainerId);
    EXPECT_EQ(state.status, shared::ContainerStatus::Running);
    ASSERT_EQ(state.recent_logs.size(), 1u);
    EXPECT_EQ(state.recent_logs[0], "log line");
    ASSERT_TRUE(state.memory_usage_mb.has_value());
    EXPECT_DOUBLE_EQ(state.memory_usage_mb.value(), 128.5);
    ASSERT_TRUE(state.cpu_usage_percent.has_value());
    EXPECT_DOUBLE_EQ(state.cpu_usage_percent.value(), 12.25);
    ASSERT_TRUE(state.container_ip.has_value());
    EXPECT_EQ(state.container_ip.value(), "10.0.0.5");
}

/**
 * @test Verifies observe skips resource metrics for non-running containers.
 */
TEST(ObservabilityEngineTests, ObserveSkipsResourceMetricsWhenNotRunning) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, getStatus(std::string_view(kContainerId)))
        .WillOnce(Return(shared::ContainerStatus::Exited));
    EXPECT_CALL(*mockEngine, getLogs(std::string_view(kContainerId))).WillOnce(Return("logs"));
    EXPECT_CALL(*mockEngine, getContainerMemoryUsage(_)).Times(0);
    EXPECT_CALL(*mockEngine, getContainerCpuUsage(_)).Times(0);
    EXPECT_CALL(*mockEngine, getContainerIp(_)).Times(0);

    observability::ObservabilityEngine obs(mockEngine);
    const auto state = obs.observe(kContainerId);

    EXPECT_EQ(state.status, shared::ContainerStatus::Exited);
    EXPECT_FALSE(state.memory_usage_mb.has_value());
    EXPECT_FALSE(state.cpu_usage_percent.has_value());
    EXPECT_FALSE(state.container_ip.has_value());
}

/**
 * @test Verifies usage lookups return nullopt on engine errors.
 */
TEST(ObservabilityEngineTests, ObserveReturnsNulloptWhenUsageLookupsFail) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, getStatus(std::string_view(kContainerId)))
        .WillOnce(Return(shared::ContainerStatus::Running));
    EXPECT_CALL(*mockEngine, getLogs(std::string_view(kContainerId))).WillOnce(Return("logs"));
    EXPECT_CALL(*mockEngine, getContainerMemoryUsage(std::string_view(kContainerId)))
        .WillOnce(Throw(containers::ContainerEngineApiError("no stats")));
    EXPECT_CALL(*mockEngine, getContainerCpuUsage(std::string_view(kContainerId)))
        .WillOnce(Throw(containers::ContainerEngineTransportError("no cpu")));
    EXPECT_CALL(*mockEngine, getContainerIp(std::string_view(kContainerId)))
        .WillOnce(Throw(containers::ContainerEngineParseError("no ip")));

    observability::ObservabilityEngine obs(mockEngine);
    const auto state = obs.observe(kContainerId);

    EXPECT_FALSE(state.memory_usage_mb.has_value());
    EXPECT_FALSE(state.cpu_usage_percent.has_value());
    EXPECT_FALSE(state.container_ip.has_value());
}

/**
 * @test Verifies observe propagates status lookup errors.
 */
TEST(ObservabilityEngineTests, ObservePropagatesStatusErrors) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, getStatus(std::string_view(kContainerId)))
        .WillOnce(Throw(containers::ContainerEngineApiError("bad status")));

    observability::ObservabilityEngine obs(mockEngine);
    EXPECT_THROW(static_cast<void>(obs.observe(kContainerId)), containers::ContainerEngineApiError);
}

/**
 * @test Verifies observe propagates log retrieval errors.
 */
TEST(ObservabilityEngineTests, ObservePropagatesLogErrors) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, getStatus(std::string_view(kContainerId)))
        .WillOnce(Return(shared::ContainerStatus::Running));
    EXPECT_CALL(*mockEngine, getLogs(std::string_view(kContainerId)))
        .WillOnce(Throw(containers::ContainerEngineTransportError("log failure")));

    observability::ObservabilityEngine obs(mockEngine);
    EXPECT_THROW(static_cast<void>(obs.observe(kContainerId)), containers::ContainerEngineTransportError);
}
