/**
 * @file RunRecorderUnitTest.cpp
 * @brief Unit tests for RunRecorder sample accumulation, phase stamping,
 *        zone boundaries, latency, logs, and finalize field population.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "core/RunResult.hpp"
#include "core/TargetState.hpp"
#include "history/RunRecord.hpp"
#include "history/RunRecorder.hpp"

using namespace chaos::orchestrator::history;
using namespace chaos::orchestrator::core;
using namespace chaos::orchestrator::manifests;

static ChaosManifest makeManifest() {
    ChaosManifest m;
    m.test_name = "test";
    m.target.id = "container-1";
    return m;
}

static TargetState makeState() {
    TargetState s;
    s.container_id = "container-1";
    s.cpu_usage_percent = 42.0;
    s.memory_usage_mb = 256.0;
    s.network_rx_bps = 1000.0;
    s.network_tx_bps = 500.0;
    return s;
}

TEST(RunRecorderTest, SampleAccumulation) {
    RunRecorder recorder(makeManifest());
    auto s1 = makeState();
    s1.cpu_usage_percent = 10.0;
    s1.memory_usage_mb = 128.0;
    recorder.onStateUpdate(s1);

    auto s2 = makeState();
    s2.cpu_usage_percent = 20.0;
    s2.memory_usage_mb = 256.0;
    recorder.onStateUpdate(s2);

    RunRecord record = recorder.finalize(RunResult{}, "completed", "");
    ASSERT_EQ(record.samples.size(), 2u);
    ASSERT_TRUE(record.samples[0].cpu_usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*record.samples[0].cpu_usage_percent, 10.0);
    ASSERT_TRUE(record.samples[0].memory_usage_mb.has_value());
    EXPECT_DOUBLE_EQ(*record.samples[0].memory_usage_mb, 128.0);
    ASSERT_TRUE(record.samples[1].cpu_usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*record.samples[1].cpu_usage_percent, 20.0);
    ASSERT_TRUE(record.samples[1].memory_usage_mb.has_value());
    EXPECT_DOUBLE_EQ(*record.samples[1].memory_usage_mb, 256.0);
    EXPECT_GT(record.samples[1].t, record.samples[0].t);
}

TEST(RunRecorderTest, PhaseStamping) {
    RunRecorder recorder(makeManifest());
    recorder.onStateUpdate(makeState());
    recorder.onPhaseChange("chaos");
    recorder.onStateUpdate(makeState());

    RunRecord record = recorder.finalize(RunResult{}, "completed", "");
    ASSERT_EQ(record.samples.size(), 2u);
    EXPECT_EQ(record.samples[0].phase, "normal");
    EXPECT_EQ(record.samples[1].phase, "chaos");
}

TEST(RunRecorderTest, ZoneBoundaryTimestamps) {
    RunRecorder recorder(makeManifest());
    recorder.onPhaseChange("chaos");
    recorder.onPhaseChange("recovery");

    RunRecord record = recorder.finalize(RunResult{}, "completed", "");
    EXPECT_GT(record.summary.normal_end_t, 0.0);
    EXPECT_GT(record.summary.chaos_end_t, record.summary.normal_end_t);
}

TEST(RunRecorderTest, LatencyStamping) {
    RunRecorder recorder(makeManifest());
    recorder.onNetworkLatencyUpdate(5.0);
    recorder.onStateUpdate(makeState());

    RunRecord record = recorder.finalize(RunResult{}, "completed", "");
    ASSERT_EQ(record.samples.size(), 1u);
    ASSERT_TRUE(record.samples[0].network_latency_ms.has_value());
    EXPECT_DOUBLE_EQ(*record.samples[0].network_latency_ms, 5.0);
}

TEST(RunRecorderTest, LogsReplaceSemantics) {
    RunRecorder recorder(makeManifest());
    recorder.onLogsUpdate({"a", "b"});
    recorder.onLogsUpdate({"c"});

    RunRecord record = recorder.finalize(RunResult{}, "completed", "");
    ASSERT_EQ(record.logs.size(), 1u);
    EXPECT_EQ(record.logs[0], "c");
}

TEST(RunRecorderTest, FinalizeCompleted) {
    RunRecorder recorder(makeManifest());
    RunRecord record = recorder.finalize(RunResult{}, "completed", "");
    EXPECT_EQ(record.summary.status, "completed");
    EXPECT_TRUE(record.summary.error.empty());
}

TEST(RunRecorderTest, FinalizeError) {
    RunRecorder recorder(makeManifest());
    RunRecord record = recorder.finalize(RunResult{}, "error", "something broke");
    EXPECT_EQ(record.summary.status, "error");
    EXPECT_EQ(record.summary.error, "something broke");
}

TEST(RunRecorderTest, FinalizeCopiesRunResult) {
    RunRecorder recorder(makeManifest());
    RunResult rr;
    rr.passed = true;
    rr.manifest_name = "my-manifest";
    rr.target_id = "tgt-1";
    rr.duration_s = 30.0;
    rr.started_at = "2024-01-01T00:00:00Z";

    RunRecord record = recorder.finalize(rr, "completed", "");
    EXPECT_TRUE(record.summary.runResult.passed);
    EXPECT_EQ(record.summary.runResult.manifest_name, "my-manifest");
    EXPECT_EQ(record.summary.runResult.target_id, "tgt-1");
    EXPECT_DOUBLE_EQ(record.summary.runResult.duration_s, 30.0);
    EXPECT_EQ(record.summary.runResult.started_at, "2024-01-01T00:00:00Z");
}

TEST(RunRecorderTest, PerturbationTypesExtracted) {
    ChaosManifest m;
    m.test_name = "test";
    m.target.id = "container-1";
    Perturbation p1;
    p1.type = "network-delay";
    m.perturbations.push_back(p1);
    Perturbation p2;
    p2.type = "kill";
    m.perturbations.push_back(p2);

    RunRecorder recorder(m);
    RunRecord record = recorder.finalize(RunResult{}, "completed", "");
    ASSERT_EQ(record.summary.perturbation_types.size(), 2u);
    EXPECT_EQ(record.summary.perturbation_types[0], "network-delay");
    EXPECT_EQ(record.summary.perturbation_types[1], "kill");
}

TEST(RunRecorderTest, IdFormat) {
    RunRecorder recorder(makeManifest());
    RunRecord record = recorder.finalize(RunResult{}, "completed", "");
    EXPECT_FALSE(record.summary.id.empty());
    EXPECT_TRUE(record.summary.id.rfind("run-", 0) == 0);
    std::string suffix = record.summary.id.substr(4);
    EXPECT_FALSE(suffix.empty());
    EXPECT_TRUE(std::all_of(suffix.begin(), suffix.end(), ::isdigit));
}
