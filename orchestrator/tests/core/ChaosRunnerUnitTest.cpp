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

    EXPECT_THROW(runner.buildPerturbations(manifest), std::invalid_argument);
}

TEST(ChaosRunnerUnitTest, ValidateExpectationsReturnsTrueWhenEmpty) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    core::ChaosRunner runner(mockEngine);

    manifests::ChaosManifest manifest;
    manifest.expectations = {};
    shared::TargetState state;

    EXPECT_TRUE(runner.validateExpectations(manifest, state));
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

    EXPECT_THROW(runner.parseManifest("/nonexistent/path/manifest.json"), manifests::ManifestParserError);
}
