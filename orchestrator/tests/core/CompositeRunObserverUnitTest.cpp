#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

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
};

TEST_F(CompositeRunObserverTest, ForwardsOnStateUpdateToAllObservers) {
    CompositeRunObserver composite({&mockA_, &mockB_});
    TargetState state;
    state.container_id = "abc123";

    EXPECT_CALL(mockA_, onStateUpdate(_)).Times(1);
    EXPECT_CALL(mockB_, onStateUpdate(_)).Times(1);

    composite.onStateUpdate(state);
}

TEST_F(CompositeRunObserverTest, ForwardsOnPhaseChangeToAllObservers) {
    CompositeRunObserver composite({&mockA_, &mockB_});

    EXPECT_CALL(mockA_, onPhaseChange(_)).Times(1);
    EXPECT_CALL(mockB_, onPhaseChange(_)).Times(1);

    composite.onPhaseChange("chaos");
}

TEST_F(CompositeRunObserverTest, ForwardsOnLogsUpdateToAllObservers) {
    CompositeRunObserver composite({&mockA_, &mockB_});
    std::vector<std::string> logs = {"line1", "line2"};

    EXPECT_CALL(mockA_, onLogsUpdate(_)).Times(1);
    EXPECT_CALL(mockB_, onLogsUpdate(_)).Times(1);

    composite.onLogsUpdate(logs);
}

TEST_F(CompositeRunObserverTest, ForwardsOnNetworkLatencyUpdateToAllObservers) {
    CompositeRunObserver composite({&mockA_, &mockB_});

    EXPECT_CALL(mockA_, onNetworkLatencyUpdate(_)).Times(1);
    EXPECT_CALL(mockB_, onNetworkLatencyUpdate(_)).Times(1);

    composite.onNetworkLatencyUpdate(12.5);
}

TEST_F(CompositeRunObserverTest, ExceptionIsolationOnStateUpdateDoesNotBlockOtherObserver) {
    CompositeRunObserver composite({&mockA_, &mockB_});
    TargetState state;

    EXPECT_CALL(mockA_, onStateUpdate(_)).WillRepeatedly(::testing::Throw(std::runtime_error("mockA state error")));
    EXPECT_CALL(mockB_, onStateUpdate(_)).Times(1);

    composite.onStateUpdate(state);
}

TEST_F(CompositeRunObserverTest, ExceptionIsolationOnPhaseChangeDoesNotBlockOtherObserver) {
    CompositeRunObserver composite({&mockA_, &mockB_});

    EXPECT_CALL(mockA_, onPhaseChange(_)).WillRepeatedly(::testing::Throw(std::runtime_error("mockA phase error")));
    EXPECT_CALL(mockB_, onPhaseChange(_)).Times(1);

    composite.onPhaseChange("recovery");
}

TEST_F(CompositeRunObserverTest, ExceptionIsolationOnLogsUpdateDoesNotBlockOtherObserver) {
    CompositeRunObserver composite({&mockA_, &mockB_});
    std::vector<std::string> logs = {"log"};

    EXPECT_CALL(mockA_, onLogsUpdate(_)).WillRepeatedly(::testing::Throw(std::runtime_error("mockA logs error")));
    EXPECT_CALL(mockB_, onLogsUpdate(_)).Times(1);

    composite.onLogsUpdate(logs);
}

TEST_F(CompositeRunObserverTest, ExceptionIsolationOnNetworkLatencyUpdateDoesNotBlockOtherObserver) {
    CompositeRunObserver composite({&mockA_, &mockB_});

    EXPECT_CALL(mockA_, onNetworkLatencyUpdate(_))
        .WillRepeatedly(::testing::Throw(std::runtime_error("mockA latency error")));
    EXPECT_CALL(mockB_, onNetworkLatencyUpdate(_)).Times(1);

    composite.onNetworkLatencyUpdate(std::nullopt);
}

TEST_F(CompositeRunObserverTest, AllMethodsForwarded) {
    CompositeRunObserver composite({&mockA_, &mockB_});
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
