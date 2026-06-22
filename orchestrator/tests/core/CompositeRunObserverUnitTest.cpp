/**
 * @file CompositeRunObserverUnitTest.cpp
 * @brief Unit tests for CompositeRunObserver forwarding and exception isolation.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "core/CompositeRunObserver.hpp"
#include "core/TargetState.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::core;
using ::testing::_;
using ::testing::StrictMock;

namespace {

class MockRunObserver : public IRunObserver {
public:
    MOCK_METHOD(void, onStateUpdate, (const core::TargetState&), (override));
    MOCK_METHOD(void, onPhaseChange, (std::string_view), (override));
    MOCK_METHOD(void, onLogsUpdate, (const std::vector<std::string>&), (override));
    MOCK_METHOD(void, onNetworkLatencyUpdate, (std::optional<double>), (override));
};

}  // namespace

class CompositeRunObserverTest : public ::testing::Test {
protected:
    StrictMock<MockRunObserver> mockA_;
    StrictMock<MockRunObserver> mockB_;
    std::vector<IRunObserver*> observers_{&mockA_, &mockB_};
};

/** @test Verifies that onStateUpdate is forwarded to every registered observer. */
TEST_F(CompositeRunObserverTest, ForwardsOnStateUpdateToAllObservers) {
    CompositeRunObserver composite(observers_);
    TargetState state;
    state.container_id = "abc123";

    EXPECT_CALL(mockA_, onStateUpdate(_)).Times(1);
    EXPECT_CALL(mockB_, onStateUpdate(_)).Times(1);

    composite.onStateUpdate(state);
}

/** @test Verifies that onPhaseChange is forwarded to every registered observer. */
TEST_F(CompositeRunObserverTest, ForwardsOnPhaseChangeToAllObservers) {
    CompositeRunObserver composite(observers_);

    EXPECT_CALL(mockA_, onPhaseChange(_)).Times(1);
    EXPECT_CALL(mockB_, onPhaseChange(_)).Times(1);

    composite.onPhaseChange("chaos");
}

/** @test Verifies that onLogsUpdate is forwarded to every registered observer. */
TEST_F(CompositeRunObserverTest, ForwardsOnLogsUpdateToAllObservers) {
    CompositeRunObserver composite(observers_);
    std::vector<std::string> logs = {"line1", "line2"};

    EXPECT_CALL(mockA_, onLogsUpdate(_)).Times(1);
    EXPECT_CALL(mockB_, onLogsUpdate(_)).Times(1);

    composite.onLogsUpdate(logs);
}

/** @test Verifies that onNetworkLatencyUpdate is forwarded to every registered observer. */
TEST_F(CompositeRunObserverTest, ForwardsOnNetworkLatencyUpdateToAllObservers) {
    CompositeRunObserver composite(observers_);

    EXPECT_CALL(mockA_, onNetworkLatencyUpdate(_)).Times(1);
    EXPECT_CALL(mockB_, onNetworkLatencyUpdate(_)).Times(1);

    composite.onNetworkLatencyUpdate(12.5);
}

/** @test Verifies that an exception in one observer during onStateUpdate does not prevent the next observer from
 * receiving the event. */
TEST_F(CompositeRunObserverTest, ExceptionIsolationOnStateUpdateDoesNotBlockOtherObserver) {
    CompositeRunObserver composite(observers_);
    TargetState state;

    EXPECT_CALL(mockA_, onStateUpdate(_)).WillRepeatedly(::testing::Throw(std::runtime_error("mockA state error")));
    EXPECT_CALL(mockB_, onStateUpdate(_)).Times(1);

    composite.onStateUpdate(state);
}

/** @test Verifies that an exception in one observer during onPhaseChange does not prevent the next observer from
 * receiving the event. */
TEST_F(CompositeRunObserverTest, ExceptionIsolationOnPhaseChangeDoesNotBlockOtherObserver) {
    CompositeRunObserver composite(observers_);

    EXPECT_CALL(mockA_, onPhaseChange(_)).WillRepeatedly(::testing::Throw(std::runtime_error("mockA phase error")));
    EXPECT_CALL(mockB_, onPhaseChange(_)).Times(1);

    composite.onPhaseChange("recovery");
}

/** @test Verifies that an exception in one observer during onLogsUpdate does not prevent the next observer from
 * receiving the event. */
TEST_F(CompositeRunObserverTest, ExceptionIsolationOnLogsUpdateDoesNotBlockOtherObserver) {
    CompositeRunObserver composite(observers_);
    std::vector<std::string> logs = {"log"};

    EXPECT_CALL(mockA_, onLogsUpdate(_)).WillRepeatedly(::testing::Throw(std::runtime_error("mockA logs error")));
    EXPECT_CALL(mockB_, onLogsUpdate(_)).Times(1);

    composite.onLogsUpdate(logs);
}

/** @test Verifies that an exception in one observer during onNetworkLatencyUpdate does not prevent the next observer
 * from receiving the event. */
TEST_F(CompositeRunObserverTest, ExceptionIsolationOnNetworkLatencyUpdateDoesNotBlockOtherObserver) {
    CompositeRunObserver composite(observers_);

    EXPECT_CALL(mockA_, onNetworkLatencyUpdate(_))
        .WillRepeatedly(::testing::Throw(std::runtime_error("mockA latency error")));
    EXPECT_CALL(mockB_, onNetworkLatencyUpdate(_)).Times(1);

    composite.onNetworkLatencyUpdate(std::nullopt);
}

/** @test Verifies that calling all four IRunObserver methods results in each being forwarded exactly once per observer.
 */
TEST_F(CompositeRunObserverTest, AllMethodsForwarded) {
    CompositeRunObserver composite(observers_);
    TargetState state;
    std::vector<std::string> logs = {"a"};

    EXPECT_CALL(mockA_, onStateUpdate(_)).Times(1);
    EXPECT_CALL(mockA_, onPhaseChange(_)).Times(1);
    EXPECT_CALL(mockA_, onLogsUpdate(_)).Times(1);
    EXPECT_CALL(mockA_, onNetworkLatencyUpdate(_)).Times(1);
    EXPECT_CALL(mockB_, onStateUpdate(_)).Times(1);
    EXPECT_CALL(mockB_, onPhaseChange(_)).Times(1);
    EXPECT_CALL(mockB_, onLogsUpdate(_)).Times(1);
    EXPECT_CALL(mockB_, onNetworkLatencyUpdate(_)).Times(1);

    composite.onStateUpdate(state);
    composite.onPhaseChange("init");
    composite.onLogsUpdate(logs);
    composite.onNetworkLatencyUpdate(42.0);
}
