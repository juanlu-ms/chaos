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

/**
 * @test Verifies buildPerturbations produces one perturbation from a single-entry manifest.
 */
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

/**
 * @test Verifies buildPerturbations handles multiple perturbation types.
 */
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

/**
 * @test Verifies buildPerturbations throws std::invalid_argument for unknown perturbation type.
 */
TEST(ChaosRunnerUnitTest, BuildPerturbationsThrowsOnUnknownType) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.test_name = "bad-type";
    manifest.target.id = "abc123";
    manifest.perturbations.push_back({.type = "nonexistent_perturbation", .parameters = {}});

    EXPECT_THROW((void)runner.buildPerturbations(manifest), std::invalid_argument);
}

/**
 * @test Verifies finalize returns passed=true when no expectations are set.
 */
TEST(ChaosRunnerUnitTest, FinalizeReturnsPassedTrueWhenEmpty) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations = {};
    core::TargetState state;

    auto result = runner.finalize(manifest, state);
    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.results.empty());
}

/**
 * @test Verifies finalize returns passed=true with no expectations and empty state.
 */
TEST(ChaosRunnerUnitTest, ValidateExpectationsReturnsTrueWhenEmpty) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations = {};
    core::TargetState state;

    EXPECT_TRUE(runner.finalize(manifest, state).passed);
}

/**
 * @test Verifies finalize returns per-expectation pass/fail results for mixed expectations.
 */
TEST(ChaosRunnerUnitTest, FinalizeReturnsDetailedResults) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    manifest.expectations.push_back({.type = "container_not_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Running;

    auto result = runner.finalize(manifest, state);
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.results.size(), 2u);
    EXPECT_TRUE(result.results[0].passed);   // container_running passes
    EXPECT_FALSE(result.results[1].passed);  // container_not_running fails
    EXPECT_EQ(result.results[0].expectation_type, "container_running");
    EXPECT_EQ(result.results[1].expectation_type, "container_not_running");
}

/**
 * @test Verifies finalize marks expectations as failed with continuous-failure message.
 */
TEST(ChaosRunnerUnitTest, FinalizeMarksContinuousFailures) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Running;

    // With a continuous failure on "container_running", the aggregate fails and
    // the per-expectation result is also marked as failed with an explanatory message.
    auto result = runner.finalize(manifest, state, {"container_running"});
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.results.size(), 1u);
    EXPECT_FALSE(result.results[0].passed);
    EXPECT_EQ(result.results[0].message, "Passed final validation but failed mid-run continuous check");
}

/**
 * @test Verifies finalize passes when all expectations are satisfied by the target state.
 */
TEST(ChaosRunnerUnitTest, ValidateExpectationsPassesWhenAllMet) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Running;

    EXPECT_TRUE(runner.finalize(manifest, state).passed);
}

/**
 * @test Verifies finalize fails when expectations are not met by the target state.
 */
TEST(ChaosRunnerUnitTest, ValidateExpectationsFailsWhenNotMet) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Exited;

    EXPECT_FALSE(runner.finalize(manifest, state).passed);
}

/**
 * @test Verifies finalize fails when some expectations pass and others fail.
 */
TEST(ChaosRunnerUnitTest, ValidateExpectationsMixedResultsFails) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    manifest.expectations.push_back({.type = "container_not_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Running;

    EXPECT_FALSE(runner.finalize(manifest, state).passed);
}

/**
 * @test Verifies parseManifest throws ManifestParserError for a nonexistent file path.
 */
TEST(ChaosRunnerUnitTest, ParseManifestThrowsOnBadPath) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    EXPECT_THROW((void)runner.parseManifest("/nonexistent/path/manifest.json"), manifests::ManifestParserError);
}
