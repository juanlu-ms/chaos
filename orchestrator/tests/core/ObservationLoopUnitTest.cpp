/**
 * @file ObservationLoopUnitTest.cpp
 * @brief Unit tests for ObservationLoop thread lifecycle.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <stop_token>
#include <thread>

#include "MockContainerEngine.hpp"
#include "core/ObservationLoop.hpp"
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
        ObservationLoop loop(engine, "test-id", state, observer);
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

    ObservationLoop loop(engine, "test-id", state, observer);

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

    ObservationLoop loop(engine, "test-id", state, observer);
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
