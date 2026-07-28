/**
 * @file ChaosServiceUnitTest.cpp
 * @brief Unit tests for ChaosService shared business logic.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "core/ChaosService.hpp"
#include "manifests/ManifestParser.hpp"

using namespace chaos::orchestrator;
using namespace testing;

/**
 * @test Verifies buildPerturbations produces one perturbation from a single-entry manifest.
 */
TEST(ChaosServiceUnitTest, BuildPerturbationsFromManifest) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.test_name = "test-build";
    manifest.target.id = "abc123";
    manifest.perturbations.push_back({.type = "cpu_cap", .parameters = {{"cpu_cores", "1"}}});

    auto perturbations = service.buildPerturbations(manifest);
    EXPECT_EQ(perturbations.size(), 1u);
}

/**
 * @test Verifies buildPerturbations handles multiple perturbation types.
 */
TEST(ChaosServiceUnitTest, BuildPerturbationsMultipleTypes) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.test_name = "multi";
    manifest.target.id = "abc123";
    manifest.perturbations.push_back({.type = "cpu_cap", .parameters = {{"cpu_cores", "1"}}});
    manifest.perturbations.push_back({.type = "memory_cap", .parameters = {{"limit_bytes", "268435456"}}});
    manifest.perturbations.push_back({.type = "kill", .parameters = {}});

    auto perturbations = service.buildPerturbations(manifest);
    EXPECT_EQ(perturbations.size(), 3u);
}

/**
 * @test Verifies buildPerturbations throws std::invalid_argument for unknown perturbation type.
 */
TEST(ChaosServiceUnitTest, BuildPerturbationsThrowsOnUnknownType) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.test_name = "bad-type";
    manifest.target.id = "abc123";
    manifest.perturbations.push_back({.type = "nonexistent_perturbation", .parameters = {}});

    EXPECT_THROW((void)service.buildPerturbations(manifest), std::invalid_argument);
}

/**
 * @test Verifies finalize returns passed=true when no expectations are set.
 */
TEST(ChaosServiceUnitTest, FinalizeReturnsPassedTrueWhenEmpty) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations = {};
    core::TargetState state;

    auto result = service.finalize(manifest, state);
    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.results.empty());
}

/**
 * @test Verifies finalize returns passed=true with no expectations and empty state.
 */
TEST(ChaosServiceUnitTest, ValidateExpectationsReturnsTrueWhenEmpty) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations = {};
    core::TargetState state;

    EXPECT_TRUE(service.finalize(manifest, state).passed);
}

/**
 * @test Verifies finalize returns per-expectation pass/fail results for mixed expectations.
 */
TEST(ChaosServiceUnitTest, FinalizeReturnsDetailedResults) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    manifest.expectations.push_back({.type = "container_not_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Running;

    auto result = service.finalize(manifest, state);
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
TEST(ChaosServiceUnitTest, FinalizeMarksContinuousFailures) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Running;

    // With a continuous failure on "container_running", the aggregate fails and
    // the per-expectation result is also marked as failed with an explanatory message.
    auto result = service.finalize(manifest, state, {"container_running"});
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.results.size(), 1u);
    EXPECT_FALSE(result.results[0].passed);
    EXPECT_EQ(result.results[0].message, "Passed final validation but failed mid-run continuous check");
}

/**
 * @test Verifies finalize keeps the final validation's own diagnosis when that check
 *       also failed, rather than claiming the expectation passed at the end.
 */
TEST(ChaosServiceUnitTest, FinalizeKeepsFinalFailureMessage) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Exited;

    // The container is dead, so the final check fails on its own. Its message is the useful
    // one and must survive; the continuous failure is appended, not substituted.
    auto result = service.finalize(manifest, state, {"container_running"});
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.results.size(), 1u);
    EXPECT_FALSE(result.results[0].passed);
    EXPECT_THAT(result.results[0].message, testing::HasSubstr("Container is NOT running"));
    EXPECT_THAT(result.results[0].message, testing::HasSubstr("also failed mid-run continuous check"));
    EXPECT_THAT(result.results[0].message, testing::Not(testing::HasSubstr("Passed final validation")));
}

/**
 * @test Verifies finalize passes when all expectations are satisfied by the target state.
 */
TEST(ChaosServiceUnitTest, ValidateExpectationsPassesWhenAllMet) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Running;

    EXPECT_TRUE(service.finalize(manifest, state).passed);
}

/**
 * @test Verifies finalize fails when expectations are not met by the target state.
 */
TEST(ChaosServiceUnitTest, ValidateExpectationsFailsWhenNotMet) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Exited;

    EXPECT_FALSE(service.finalize(manifest, state).passed);
}

/**
 * @test Verifies finalize fails when some expectations pass and others fail.
 */
TEST(ChaosServiceUnitTest, ValidateExpectationsMixedResultsFails) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations.push_back({.type = "container_running", .parameters = {}});
    manifest.expectations.push_back({.type = "container_not_running", .parameters = {}});
    core::TargetState state;
    state.status = containers::ContainerStatus::Running;

    EXPECT_FALSE(service.finalize(manifest, state).passed);
}

/**
 * @test Verifies parseManifest throws ManifestParserError for a nonexistent file path.
 */
TEST(ChaosServiceUnitTest, ParseManifestThrowsOnBadPath) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosService service(mockEngine);

    EXPECT_THROW((void)service.parseManifest("/nonexistent/path/manifest.json"), manifests::ManifestParserError);
}
