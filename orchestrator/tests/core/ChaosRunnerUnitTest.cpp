/**
 * @file ChaosRunnerUnitTest.cpp
 * @brief Unit tests for ChaosRunner shared business logic.
 */

#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "core/ChaosRunner.hpp"
#include "manifests/ManifestParser.hpp"

using namespace chaos::orchestrator;
using namespace testing;

TEST(ChaosRunnerUnitTest, BuildPerturbationsFromManifest) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.test_name = "test-build";
    manifest.target.id = "abc123";
    manifest.perturbations.push_back({.type = "cpu_cap", .parameters = {{"cpu_cores", "1"}}});

    auto perturbations = runner.buildPerturbations(manifest);
    EXPECT_EQ(perturbations.size(), 1u);
}

TEST(ChaosRunnerUnitTest, BuildPerturbationsMultipleTypes) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.test_name = "multi";
    manifest.target.id = "abc123";
    manifest.perturbations.push_back({.type = "cpu_cap", .parameters = {{"cpu_cores", "1"}}});
    manifest.perturbations.push_back({.type = "memory_cap", .parameters = {{"limit_bytes", "268435456"}}});
    manifest.perturbations.push_back({.type = "kill", .parameters = {}});

    auto perturbations = runner.buildPerturbations(manifest);
    EXPECT_EQ(perturbations.size(), 3u);
}

TEST(ChaosRunnerUnitTest, BuildPerturbationsThrowsOnUnknownType) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.test_name = "bad-type";
    manifest.target.id = "abc123";
    manifest.perturbations.push_back({.type = "nonexistent_perturbation", .parameters = {}});

    EXPECT_THROW((void)runner.buildPerturbations(manifest), std::invalid_argument);
}

TEST(ChaosRunnerUnitTest, FinalizeReturnsPassedTrueWhenEmpty) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations = {};
    shared::TargetState state;

    auto result = runner.finalize(manifest, state);
    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.results.empty());
}

TEST(ChaosRunnerUnitTest, ValidateExpectationsReturnsTrueWhenEmpty) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations = {};
    shared::TargetState state;

    EXPECT_TRUE(runner.validateExpectations(manifest, state));
}

TEST(ChaosRunnerUnitTest, FinalizeReturnsDetailedResults) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    manifest.expectations.push_back({.type = "container_not_running", .parameters = {}});
    shared::TargetState state;
    state.status = shared::ContainerStatus::Running;

    auto result = runner.finalize(manifest, state);
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.results.size(), 2u);
    EXPECT_TRUE(result.results[0].passed);   // container_running passes
    EXPECT_FALSE(result.results[1].passed);  // container_not_running fails
    EXPECT_EQ(result.results[0].expectationType, "container_running");
    EXPECT_EQ(result.results[1].expectationType, "container_not_running");
}

TEST(ChaosRunnerUnitTest, FinalizeMarksContinuousFailures) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    shared::TargetState state;
    state.status = shared::ContainerStatus::Running;

    // With a continuous failure on "container_running", the aggregate fails and
    // the per-expectation result is also marked as failed with an explanatory message.
    auto result = runner.finalize(manifest, state, {"container_running"});
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.results.size(), 1u);
    EXPECT_FALSE(result.results[0].passed);
    EXPECT_EQ(result.results[0].message, "Passed final validation but failed mid-run continuous check");
}

TEST(ChaosRunnerUnitTest, ValidateExpectationsPassesWhenAllMet) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    shared::TargetState state;
    state.status = shared::ContainerStatus::Running;

    EXPECT_TRUE(runner.validateExpectations(manifest, state));
}

TEST(ChaosRunnerUnitTest, ValidateExpectationsFailsWhenNotMet) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    shared::TargetState state;
    state.status = shared::ContainerStatus::Exited;

    EXPECT_FALSE(runner.validateExpectations(manifest, state));
}

TEST(ChaosRunnerUnitTest, ValidateExpectationsMixedResultsFails) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    manifest.expectations.push_back({.type = "container_not_running", .parameters = {}});
    shared::TargetState state;
    state.status = shared::ContainerStatus::Running;

    EXPECT_FALSE(runner.validateExpectations(manifest, state));
}

TEST(ChaosRunnerUnitTest, ParseManifestThrowsOnBadPath) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    EXPECT_THROW((void)runner.parseManifest("/nonexistent/path/manifest.json"), manifests::ManifestParserError);
}
