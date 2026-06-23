/**
 * @file RunOrchestratorE2eTest.cpp
 * @brief End-to-end tests verifying the full orchestration path:
 *        parse manifest → observe → run chaos → validate.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "E2eTestBase.hpp"
#include "core/ChaosRunner.hpp"
#include "core/IRunObserver.hpp"
#include "core/ObservationLoop.hpp"
#include "core/RunOrchestrator.hpp"
#include "core/SharedState.hpp"
#include "manifests/Manifest.hpp"
#include "manifests/ManifestParser.hpp"

using namespace chaos::orchestrator;

namespace {

class E2eRunObserver final : public core::IRunObserver {
public:
    void onStateUpdate(const core::TargetState&) override { ++state_updates; }
    void onPhaseChange(std::string_view phase) override { last_phase = phase; }
    void onLogsUpdate(const std::vector<std::string>&) override { ++log_updates; }

    int state_updates = 0;
    int log_updates = 0;
    std::string last_phase;
};

}  // namespace

class RunOrchestratorE2eTest : public E2eTestBase {};

/**
 * @test Full orchestration lifecycle: manifest with no perturbations,
 *       expects container_running, verifies pass.
 */
TEST_F(RunOrchestratorE2eTest, NoPerturbationsPassesRunningExpectation) {
    std::string json = R"({
        "test_name": "e2e-orch-no-pert",
        "target": {"id": ")" +
                       containerId() + R"("},
        "perturbations": [],
        "expectations": [{"type": "container_running", "parameters": {}}]
    })";

    auto manifest = manifests::ManifestParser::parseFromJson(json);

    core::SharedState state;
    E2eRunObserver observer;

    core::ObservationLoop::Config loop_config;
    loop_config.metricsInterval = std::chrono::milliseconds(200);
    loop_config.logsInterval = std::chrono::milliseconds(2000);
    loop_config.continuous_expectations = manifest.expectations;

    core::ChaosRunner runner(engine());
    core::RunOrchestrator orchestrator(runner);

    {
        core::ObservationLoop obs_loop(engine(), manifest.target.id, state, observer, loop_config);
        obs_loop.start();

        auto result = orchestrator.run(manifest, state, observer, {});
        EXPECT_TRUE(result.passed);
    }

    EXPECT_EQ(observer.last_phase, "recovery");
    EXPECT_GT(observer.state_updates, 0);
}
