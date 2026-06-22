/**
 * @file HistoryJsonUnitTest.cpp
 * @brief Unit tests for RunRecord and RunSummary JSON round-trip serialization.
 */

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "core/ResultSerializer.hpp"
#include "history/HistoryJson.hpp"
#include "history/RunRecord.hpp"
#include "validation/ValidationResult.hpp"

using namespace chaos::orchestrator::history;
using namespace chaos::orchestrator::core;
using namespace chaos::orchestrator::manifests;
using namespace chaos::orchestrator::validation;

/**
 * @test Full RunRecord → JSON → RunRecord round-trip with all fields populated.
 */
TEST(HistoryJsonTest, RunRecordRoundTrip) {
    RunRecord original;
    original.summary.id = "run-1719000000000";
    original.summary.started_at_unix = 1719000000000;
    original.summary.ended_at_unix = 1719000010000;
    original.summary.status = "completed";
    original.summary.error = "";
    original.summary.perturbation_types = {"network_delay", "cpu_cap"};
    original.summary.normal_end_t = 5.0;
    original.summary.chaos_end_t = 25.0;
    original.summary.runResult.passed = true;
    original.summary.runResult.manifest_name = "test-run-1";
    original.summary.runResult.target_id = "container-abc";
    original.summary.runResult.duration_s = 30.0;
    original.summary.runResult.started_at = "2024-06-21T12:00:00Z";
    {
        ValidationResult vr;
        vr.passed = true;
        vr.expectationType = "container_running";
        vr.message = "container is running";
        original.summary.runResult.results.push_back(vr);
    }

    original.manifest.test_name = "test-run-1";
    original.manifest.target.id = "container-abc";
    original.manifest.duration_s = 30;

    Perturbation pert;
    pert.type = "network_delay";
    pert.parameters["delay_ms"] = "100";
    original.manifest.perturbations.push_back(pert);

    Expectation exp;
    exp.type = "container_running";
    exp.continuous = true;
    original.manifest.expectations.push_back(exp);

    RunSample sample;
    sample.t = 10.0;
    sample.phase = "chaos";
    sample.cpu_usage_percent = 45.5;
    sample.memory_usage_mb = 128.0;
    sample.network_rx_bps = 1000.0;
    sample.network_tx_bps = 500.0;
    sample.network_latency_ms = 5.2;
    original.samples.push_back(sample);

    original.logs = {"line one", "line two"};

    auto j = runRecordToJson(original);
    auto parsed = runRecordFromJson(j);
    ASSERT_TRUE(parsed.has_value());

    const auto& p = *parsed;
    EXPECT_EQ(p.summary.id, original.summary.id);
    EXPECT_EQ(p.summary.started_at_unix, original.summary.started_at_unix);
    EXPECT_EQ(p.summary.ended_at_unix, original.summary.ended_at_unix);
    EXPECT_EQ(p.summary.status, original.summary.status);
    EXPECT_EQ(p.summary.error, original.summary.error);
    EXPECT_EQ(p.summary.perturbation_types, original.summary.perturbation_types);
    EXPECT_DOUBLE_EQ(p.summary.normal_end_t, original.summary.normal_end_t);
    EXPECT_DOUBLE_EQ(p.summary.chaos_end_t, original.summary.chaos_end_t);
    EXPECT_EQ(p.summary.runResult.passed, original.summary.runResult.passed);
    EXPECT_EQ(p.summary.runResult.manifest_name, original.summary.runResult.manifest_name);
    EXPECT_EQ(p.summary.runResult.target_id, original.summary.runResult.target_id);
    EXPECT_DOUBLE_EQ(p.summary.runResult.duration_s, original.summary.runResult.duration_s);
    EXPECT_EQ(p.summary.runResult.started_at, original.summary.runResult.started_at);
    ASSERT_EQ(p.summary.runResult.results.size(), 1u);
    EXPECT_EQ(p.summary.runResult.results[0].passed, true);
    EXPECT_EQ(p.summary.runResult.results[0].expectationType, "container_running");
    EXPECT_EQ(p.summary.runResult.results[0].message, "container is running");

    EXPECT_EQ(p.manifest.test_name, original.manifest.test_name);
    EXPECT_EQ(p.manifest.target.id, original.manifest.target.id);
    ASSERT_TRUE(p.manifest.duration_s.has_value());
    EXPECT_EQ(*p.manifest.duration_s, *original.manifest.duration_s);
    ASSERT_EQ(p.manifest.perturbations.size(), 1u);
    EXPECT_EQ(p.manifest.perturbations[0].type, "network_delay");
    EXPECT_EQ(p.manifest.perturbations[0].parameters.at("delay_ms"), "100");
    ASSERT_EQ(p.manifest.expectations.size(), 1u);
    EXPECT_EQ(p.manifest.expectations[0].type, "container_running");
    EXPECT_EQ(p.manifest.expectations[0].continuous, true);

    ASSERT_EQ(p.samples.size(), 1u);
    EXPECT_DOUBLE_EQ(p.samples[0].t, 10.0);
    EXPECT_EQ(p.samples[0].phase, "chaos");
    ASSERT_TRUE(p.samples[0].cpu_usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*p.samples[0].cpu_usage_percent, 45.5);
    ASSERT_TRUE(p.samples[0].memory_usage_mb.has_value());
    EXPECT_DOUBLE_EQ(*p.samples[0].memory_usage_mb, 128.0);
    ASSERT_TRUE(p.samples[0].network_rx_bps.has_value());
    EXPECT_DOUBLE_EQ(*p.samples[0].network_rx_bps, 1000.0);
    ASSERT_TRUE(p.samples[0].network_tx_bps.has_value());
    EXPECT_DOUBLE_EQ(*p.samples[0].network_tx_bps, 500.0);
    ASSERT_TRUE(p.samples[0].network_latency_ms.has_value());
    EXPECT_DOUBLE_EQ(*p.samples[0].network_latency_ms, 5.2);

    ASSERT_EQ(p.logs.size(), 2u);
    EXPECT_EQ(p.logs[0], "line one");
    EXPECT_EQ(p.logs[1], "line two");
}

/**
 * @test RunSummary → JSON → RunSummary round-trip.
 */
TEST(HistoryJsonTest, RunSummaryRoundTrip) {
    RunSummary original;
    original.id = "run-1719000000000";
    original.started_at_unix = 1719000000000;
    original.ended_at_unix = 1719000010000;
    original.status = "completed";
    original.error = "";
    original.perturbation_types = {"kill"};
    original.normal_end_t = 3.0;
    original.chaos_end_t = 18.0;
    original.runResult.passed = false;
    original.runResult.manifest_name = "kill-test";
    original.runResult.target_id = "target-1";
    original.runResult.duration_s = 20.0;
    original.runResult.started_at = "2024-06-21T12:00:00Z";
    {
        ValidationResult vr;
        vr.passed = false;
        vr.expectationType = "container_not_running";
        vr.message = "container was still running";
        original.runResult.results.push_back(vr);
    }

    auto j = runSummaryToJson(original);
    auto parsed = runSummaryFromJson(j);
    ASSERT_TRUE(parsed.has_value());

    const auto& p = *parsed;
    EXPECT_EQ(p.id, original.id);
    EXPECT_EQ(p.started_at_unix, original.started_at_unix);
    EXPECT_EQ(p.ended_at_unix, original.ended_at_unix);
    EXPECT_EQ(p.status, original.status);
    EXPECT_EQ(p.error, original.error);
    EXPECT_EQ(p.perturbation_types, original.perturbation_types);
    EXPECT_DOUBLE_EQ(p.normal_end_t, original.normal_end_t);
    EXPECT_DOUBLE_EQ(p.chaos_end_t, original.chaos_end_t);
    EXPECT_EQ(p.runResult.passed, original.runResult.passed);
    EXPECT_EQ(p.runResult.manifest_name, original.runResult.manifest_name);
    EXPECT_EQ(p.runResult.target_id, original.runResult.target_id);
    EXPECT_DOUBLE_EQ(p.runResult.duration_s, original.runResult.duration_s);
    EXPECT_EQ(p.runResult.started_at, original.runResult.started_at);
    ASSERT_EQ(p.runResult.results.size(), 1u);
    EXPECT_EQ(p.runResult.results[0].passed, false);
    EXPECT_EQ(p.runResult.results[0].expectationType, "container_not_running");
    EXPECT_EQ(p.runResult.results[0].message, "container was still running");
}

/**
 * @test Verifies that the RunResult embedded in RunSummary serializes the same way as core::runResultToJson.
 */
TEST(HistoryJsonTest, EmbeddedRunResultMatchesCoreSerializer) {
    RunResult rr;
    rr.passed = true;
    rr.manifest_name = "compare-test";
    rr.target_id = "tgt-42";
    rr.duration_s = 10.5;
    rr.started_at = "2024-06-21T15:00:00Z";
    {
        ValidationResult vr;
        vr.passed = true;
        vr.expectationType = "log_contains";
        vr.message = "found expected log line";
        rr.results.push_back(vr);
    }

    RunSummary summary;
    summary.id = "run-1719000000000";
    summary.started_at_unix = 1719000000000;
    summary.ended_at_unix = 1719000010500;
    summary.status = "completed";
    summary.normal_end_t = 3.0;
    summary.chaos_end_t = 8.0;
    summary.runResult = rr;

    auto summaryJson = runSummaryToJson(summary);
    auto coreJson = runResultToJson(rr);

    EXPECT_EQ(summaryJson["passed"], coreJson["passed"]);
    EXPECT_EQ(summaryJson["manifest_name"], coreJson["manifest_name"]);
    EXPECT_EQ(summaryJson["target_id"], coreJson["target_id"]);
    EXPECT_EQ(summaryJson["duration_s"], coreJson["duration_s"]);
    EXPECT_EQ(summaryJson["started_at"], coreJson["started_at"]);
    ASSERT_TRUE(summaryJson["results"].is_array());
    ASSERT_TRUE(coreJson["results"].is_array());
    ASSERT_EQ(summaryJson["results"].size(), coreJson["results"].size());
    for (size_t i = 0; i < summaryJson["results"].size(); ++i) {
        EXPECT_EQ(summaryJson["results"][i]["type"], coreJson["results"][i]["type"]);
        EXPECT_EQ(summaryJson["results"][i]["passed"], coreJson["results"][i]["passed"]);
        EXPECT_EQ(summaryJson["results"][i]["message"], coreJson["results"][i]["message"]);
    }
}

/**
 * @test Parsing invalid JSON (a bare integer) returns nullopt.
 */
TEST(HistoryJsonTest, ParseInvalidJsonReturnsNullopt) {
    nlohmann::json j = 42;
    EXPECT_FALSE(runRecordFromJson(j).has_value());
    EXPECT_FALSE(runSummaryFromJson(j).has_value());
}

/**
 * @test Parsing JSON with missing required fields returns nullopt.
 */
TEST(HistoryJsonTest, ParseJsonMissingFieldsReturnsNullopt) {
    nlohmann::json j;
    j["summary"] = nlohmann::json::object();
    j["manifest"] = nlohmann::json::object();
    EXPECT_FALSE(runRecordFromJson(j).has_value());

    nlohmann::json emptyObj = nlohmann::json::object();
    EXPECT_FALSE(runSummaryFromJson(emptyObj).has_value());
}
