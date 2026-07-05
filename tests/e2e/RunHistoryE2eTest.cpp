/**
 * @file RunHistoryE2eTest.cpp
 * @brief End-to-end test for run history persistence and retrieval.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "E2eTestBase.hpp"
#include "core/ChaosService.hpp"
#include "core/CompositeRunObserver.hpp"
#include "core/IRunObserver.hpp"
#include "core/ObservationLoop.hpp"
#include "core/RunOrchestrator.hpp"
#include "core/SharedState.hpp"
#include "history/FileRunHistory.hpp"
#include "history/RunRecorder.hpp"
#include "manifests/Manifest.hpp"
#include "manifests/ManifestParser.hpp"

using namespace chaos::orchestrator;

namespace {

class E2eRunObserver final : public core::IRunObserver {
public:
    void onStateUpdate(const core::TargetState&) override { ++state_updates; }
    void onPhaseChange(std::string_view phase) override { last_phase = phase; }
    void onLogsUpdate(const std::vector<std::string>&) override { ++log_updates; }
    void onNetworkLatencyUpdate(std::optional<double>) override {}

    int state_updates = 0;
    int log_updates = 0;
    std::string last_phase;
};

}  // namespace

class RunHistoryE2eTest : public E2eTestBase {
protected:
    std::filesystem::path temp_dir_;

    void SetUp() override {
        E2eTestBase::SetUp();
        temp_dir_ = std::filesystem::temp_directory_path() / ("chaos_e2e_history_" + std::to_string(std::rand()));
        std::filesystem::create_directories(temp_dir_);
    }
    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
        E2eTestBase::TearDown();
    }
};

/** @test Runs a chaos test end-to-end with observation, saves to file history, and verifies the record is retrievable
 * with correct fields. */
TEST_F(RunHistoryE2eTest, FullRunSavesToHistory) {
    std::string json = R"({
        "test_name": "e2e-history-test",
        "target": {"id": ")" +
                       containerId() + R"("},
        "perturbations": [],
        "expectations": [{"type": "container_running", "parameters": {}}],
        "duration_s": 1
    })";

    auto manifest = manifests::ManifestParser::parseFromJson(json);

    auto history = std::make_shared<history::FileRunHistory>(temp_dir_, 10);

    core::SharedState state;
    E2eRunObserver observer;
    history::RunRecorder recorder(manifest);
    core::CompositeRunObserver composite(std::vector<core::IRunObserver*>{&observer, &recorder});

    core::ObservationLoop::Config loop_config;
    loop_config.metricsInterval = std::chrono::milliseconds(200);
    loop_config.logsInterval = std::chrono::milliseconds(2000);
    loop_config.continuous_expectations = manifest.expectations;

    core::ChaosService service(engine());
    core::RunOrchestrator orchestrator(service);

    {
        core::ObservationLoop obs_loop(engine(), manifest.target.id, state, composite, loop_config);
        obs_loop.start();

        auto result = orchestrator.run(manifest, state, composite, {});
        EXPECT_TRUE(result.passed);

        history->save(recorder.finalize(result, "completed", ""));
    }

    auto list = history->list();
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].status, "completed");
    EXPECT_EQ(list[0].run_result.manifest_name, "e2e-history-test");
    EXPECT_TRUE(list[0].run_result.passed);

    auto record = history->get(list[0].id);
    ASSERT_TRUE(record.has_value());
    EXPECT_FALSE(record->samples.empty());
    EXPECT_EQ(record->summary.perturbation_types.size(), 0u);

    EXPECT_GT(record->summary.normal_end_t, 0.0);
    EXPECT_GT(record->summary.chaos_end_t, 0.0);
    EXPECT_GT(record->summary.chaos_end_t, record->summary.normal_end_t);
}
