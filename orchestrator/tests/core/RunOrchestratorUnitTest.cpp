/**
 * @file RunOrchestratorUnitTest.cpp
 * @brief Unit tests for RunOrchestrator lifecycle engine.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string_view>
#include <thread>
#include <vector>

#include "MockContainerEngine.hpp"
#include "core/RunOrchestrator.hpp"
#include "core/SharedState.hpp"
#include "perturbations/IPerturbation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::core;
using namespace testing;
using namespace std::chrono_literals;

namespace {

/**
 * @brief GMock-based observer for verifying phase/state/log callbacks.
 */
class MockRunObserver final : public core::IRunObserver {
public:
    MOCK_METHOD(void, onStateUpdate, (const core::TargetState&), (override));
    MOCK_METHOD(void, onLogsUpdate, (const std::vector<std::string>&), (override));
    MOCK_METHOD(void, onPhaseChange, (std::string_view), (override));
};

/**
 * @brief Perturbation whose apply() always fails, i.e. the fault never reaches the target.
 */
class FailingPerturbation final : public perturbations::IPerturbation {
public:
    void apply() override { throw std::runtime_error("engine unreachable"); }
    void revert() override {}
    [[nodiscard]] std::string_view type() const override { return "kill"; }
};

[[nodiscard]] std::vector<std::unique_ptr<perturbations::IPerturbation>> makeFailingPerturbation() {
    std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations;
    perturbations.push_back(std::make_unique<FailingPerturbation>());
    return perturbations;
}

}  // namespace

/**
 * @test With no duration, chaos phase is skipped entirely.
 */
TEST(RunOrchestratorTest, RunWithNoDurationSkipsChaosPhase) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    ChaosService service(engine);
    RunOrchestrator orchestrator(service);

    manifests::ChaosManifest manifest;
    manifest.test_name = "no-duration";
    manifest.target.id = "test-container";
    manifest.duration_s = std::nullopt;

    SharedState state;
    MockRunObserver observer;

    EXPECT_CALL(observer, onPhaseChange(StrEq("normal"))).Times(1);
    EXPECT_CALL(observer, onPhaseChange(StrEq("chaos"))).Times(0);
    EXPECT_CALL(observer, onPhaseChange(StrEq("recovery"))).Times(1);

    auto result = orchestrator.run(manifest, state, observer, {});
    EXPECT_TRUE(result.passed);
}

/**
 * @test With a duration, all three phases are entered in order.
 */
TEST(RunOrchestratorTest, RunWithDurationEntersAllPhases) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    ChaosService service(engine);
    RunOrchestrator orchestrator(service);

    manifests::ChaosManifest manifest;
    manifest.test_name = "quick-run";
    manifest.target.id = "test-container";
    manifest.duration_s = 1;

    SharedState state;
    MockRunObserver observer;

    EXPECT_CALL(observer, onPhaseChange(StrEq("normal"))).Times(1);
    EXPECT_CALL(observer, onPhaseChange(StrEq("chaos"))).Times(1);
    EXPECT_CALL(observer, onPhaseChange(StrEq("recovery"))).Times(1);

    auto result = orchestrator.run(manifest, state, observer, {});
    EXPECT_TRUE(result.passed);
}

/**
 * @test External stop cancels long-running chaos phase early.
 *       The stop is requested during the normal-phase sleep, so chaos is
 *       skipped entirely.
 */
TEST(RunOrchestratorTest, ExternalStopCancelsEarly) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    ChaosService service(engine);
    RunOrchestrator orchestrator(service);

    manifests::ChaosManifest manifest;
    manifest.test_name = "cancel-test";
    manifest.target.id = "test-container";
    manifest.duration_s = 60;

    SharedState state;
    MockRunObserver observer;

    EXPECT_CALL(observer, onPhaseChange(StrEq("normal"))).Times(1);
    EXPECT_CALL(observer, onPhaseChange(StrEq("chaos"))).Times(0);
    EXPECT_CALL(observer, onPhaseChange(StrEq("recovery"))).Times(1);

    std::stop_source stop_src;

    auto start = std::chrono::steady_clock::now();

    std::thread cancel_thread([&stop_src] {
        std::this_thread::sleep_for(50ms);
        stop_src.request_stop();
    });

    auto result = orchestrator.run(manifest, state, observer, {}, stop_src.get_token());
    cancel_thread.join();

    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, 5s);
    EXPECT_TRUE(result.passed);
}

/**
 * @test A perturbation that fails to apply marks the run as failed and reports the
 *       perturbation type, since the fault was never injected.
 */
TEST(RunOrchestratorTest, PerturbationApplyFailureFailsRun) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    ChaosService service(engine);
    RunOrchestrator orchestrator(service);

    manifests::ChaosManifest manifest;
    manifest.test_name = "apply-failure";
    manifest.target.id = "test-container";
    manifest.duration_s = 1;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});

    SharedState state;
    MockRunObserver observer;

    auto result = orchestrator.run(manifest, state, observer, makeFailingPerturbation());

    EXPECT_FALSE(result.passed);
    const auto failure =
        std::ranges::find_if(result.results, [](const auto& res) { return res.expectation_type == "perturbation"; });
    ASSERT_NE(failure, result.results.end());
    EXPECT_FALSE(failure->passed);
    EXPECT_THAT(failure->message, HasSubstr("kill (#0)"));
    EXPECT_THAT(failure->message, HasSubstr("engine unreachable"));
}

/**
 * @test A failed apply() also fails a run with no expectations, where finalize()
 *        would otherwise report a pass by default.
 */
TEST(RunOrchestratorTest, PerturbationApplyFailureFailsRunWithoutExpectations) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    ChaosService service(engine);
    RunOrchestrator orchestrator(service);

    manifests::ChaosManifest manifest;
    manifest.test_name = "apply-failure-no-expectations";
    manifest.target.id = "test-container";
    manifest.duration_s = 1;

    SharedState state;
    MockRunObserver observer;

    auto result = orchestrator.run(manifest, state, observer, makeFailingPerturbation());

    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.results.size(), 1U);
    EXPECT_EQ(result.results.front().expectation_type, "perturbation");
}

/**
 * @test Continuous failures from SharedState cause finalize() to return failed
 *        when matching expectations exist in the manifest.
 */
TEST(RunOrchestratorTest, ReadsContinuousFailuresFromSharedState) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    ChaosService service(engine);
    RunOrchestrator orchestrator(service);

    manifests::ChaosManifest manifest;
    manifest.test_name = "continuous-fail";
    manifest.target.id = "test-container";
    manifest.duration_s = std::nullopt;
    manifest.expectations.push_back({.type = "http_status", .parameters = {}});

    SharedState state;
    state.addContinuousFailure("http_status");

    MockRunObserver observer;
    EXPECT_CALL(observer, onPhaseChange(StrEq("normal"))).Times(1);
    EXPECT_CALL(observer, onPhaseChange(StrEq("recovery"))).Times(1);

    auto result = orchestrator.run(manifest, state, observer, {});

    EXPECT_FALSE(result.passed);
}

/**
 * @test RunResult carries manifest metadata copied from the manifest.
 */
TEST(RunOrchestratorTest, PopulatesRunMetadata) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    ChaosService service(engine);
    RunOrchestrator orchestrator(service);

    manifests::ChaosManifest manifest;
    manifest.test_name = "meta-test";
    manifest.target.id = "meta-container";
    manifest.duration_s = std::nullopt;

    SharedState state;
    MockRunObserver observer;

    auto result = orchestrator.run(manifest, state, observer, {});

    EXPECT_EQ(result.manifest_name, "meta-test");
    EXPECT_EQ(result.target_id, "meta-container");
    EXPECT_FALSE(result.started_at.empty());
    EXPECT_GE(result.duration_s, 0.0);
}
