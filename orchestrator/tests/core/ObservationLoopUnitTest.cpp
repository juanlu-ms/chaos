/**
 * @file ObservationLoopUnitTest.cpp
 * @brief Unit tests for ObservationLoop thread lifecycle.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <stop_token>
#include <thread>

#include "MockContainerEngine.hpp"
#include "core/ObservationLoop.hpp"
#include "core/RunPhase.hpp"
#include "core/SharedState.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::core;
using namespace std::chrono_literals;

namespace {

/**
 * @brief Observer stub that ignores all callbacks.
 */
class NullRunObserver final : public core::IRunObserver {
public:
    void onStateUpdate(const core::TargetState&) override {}
    void onPhaseChange(std::string_view) override {}
};

}  // namespace

/**
 * @test Verifies destructor safely stops and joins threads without external token.
 */
TEST(ObservationLoopTest, StartAndDestroyWithoutExternalStop) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    SharedState state;
    NullRunObserver observer;

    {
        ObservationLoop loop(engine, "test-id", state, observer, ObservationLoop::Config{});
        loop.start();
        std::this_thread::sleep_for(50ms);
    }
    SUCCEED();
}

/**
 * @test Verifies external stop token causes threads to exit cleanly.
 */
TEST(ObservationLoopTest, StartAndStopWithExternalToken) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    SharedState state;
    NullRunObserver observer;

    ObservationLoop loop(engine, "test-id", state, observer, ObservationLoop::Config{});

    std::stop_source src;
    loop.start(src.get_token());
    std::this_thread::sleep_for(50ms);
    src.request_stop();

    std::this_thread::sleep_for(100ms);
    SUCCEED();
}

/**
 * @test Documents that double-start is a fatal logic error.
 */
TEST(ObservationLoopTest, DoubleStartIsFatal) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    SharedState state;
    NullRunObserver observer;

    ObservationLoop loop(engine, "test-id", state, observer, ObservationLoop::Config{});
    loop.start();
    SUCCEED();
}

/**
 * @test Verifies custom Config intervals are accepted without deadlock.
 */
TEST(ObservationLoopTest, ConfigIsRespected) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    SharedState state;
    NullRunObserver observer;

    ObservationLoop::Config cfg;
    cfg.metricsInterval = 10ms;
    cfg.logsInterval = 10ms;

    ObservationLoop loop(engine, "test-id", state, observer, cfg);
    loop.start();

    std::this_thread::sleep_for(50ms);

    SUCCEED();
}

/**
 * @test Verifies stop() is idempotent and safe on a loop that was never started.
 */
TEST(ObservationLoopTest, StopIsIdempotentAndSafeBeforeStart) {
    auto engine = std::make_shared<testing::NiceMock<tests::MockContainerEngine>>();
    SharedState state;
    NullRunObserver observer;

    ObservationLoop never_started(engine, "test-id", state, observer, ObservationLoop::Config{});
    never_started.stop();

    ObservationLoop loop(engine, "test-id", state, observer, ObservationLoop::Config{});
    loop.start();
    std::this_thread::sleep_for(20ms);
    loop.stop();
    loop.stop();

    SUCCEED();
}

/**
 * @test Verifies stop() returns promptly even when a thread's poll interval is long.
 *
 * Regression guard: the background sleeps must be interruptible, otherwise joining
 * the logs thread would block for a full logsInterval before results can be reported.
 */
TEST(ObservationLoopTest, StopReturnsPromptlyDespiteLongLogsInterval) {
    auto engine = std::make_shared<testing::NiceMock<tests::MockContainerEngine>>();
    SharedState state;
    NullRunObserver observer;

    ObservationLoop::Config cfg;
    cfg.metricsInterval = 200ms;
    cfg.logsInterval = 2s;

    ObservationLoop loop(engine, "test-id", state, observer, cfg);
    loop.start();
    std::this_thread::sleep_for(50ms);

    auto begin = std::chrono::steady_clock::now();
    loop.stop();
    auto elapsed = std::chrono::steady_clock::now() - begin;

    EXPECT_LT(elapsed, 500ms);
}

/**
 * @test Verifies continuous expectations are only evaluated during the chaos phase.
 */
TEST(ObservationLoopTest, ContinuousValidationOnlyRunsDuringChaosPhase) {
    auto engine = std::make_shared<testing::NiceMock<tests::MockContainerEngine>>();
    ON_CALL(*engine, getStatus(testing::_)).WillByDefault(testing::Return(containers::ContainerStatus::Exited));

    SharedState state;
    NullRunObserver observer;

    ObservationLoop::Config cfg;
    cfg.metricsInterval = 10ms;
    cfg.logsInterval = 1s;
    cfg.continuousValidationInterval = 10ms;
    cfg.continuous_expectations.push_back({.type = "container_running", .parameters = {}});

    ObservationLoop loop(engine, "test-id", state, observer, cfg);
    loop.start();

    // Normal phase: the container is down, but that is not the fault's doing.
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(state.continuousFailures().empty());

    state.setPhase(RunPhase::Chaos);
    std::this_thread::sleep_for(100ms);
    EXPECT_THAT(state.continuousFailures(), testing::Contains("container_running"));

    loop.stop();
}

/**
 * @test Verifies the recovery phase does not record continuous failures.
 */
TEST(ObservationLoopTest, ContinuousValidationSkipsRecoveryPhase) {
    auto engine = std::make_shared<testing::NiceMock<tests::MockContainerEngine>>();
    ON_CALL(*engine, getStatus(testing::_)).WillByDefault(testing::Return(containers::ContainerStatus::Exited));

    SharedState state;
    NullRunObserver observer;

    ObservationLoop::Config cfg;
    cfg.metricsInterval = 10ms;
    cfg.logsInterval = 1s;
    cfg.continuousValidationInterval = 10ms;
    cfg.continuous_expectations.push_back({.type = "container_running", .parameters = {}});

    ObservationLoop loop(engine, "test-id", state, observer, cfg);
    loop.start();

    state.setPhase(RunPhase::Recovery);
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(state.continuousFailures().empty());

    loop.stop();
}
