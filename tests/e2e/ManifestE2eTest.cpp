/**
 * @file ManifestE2eTest.cpp
 * @brief End-to-end tests for manifest parsing and execution.
 */

#include <gtest/gtest.h>

#include "E2eTestBase.hpp"
#include "manifests/Manifest.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "validation/ValidationEngine.hpp"

using chaos::orchestrator::manifests::Expectation;
using chaos::orchestrator::manifests::Parameters;
using chaos::orchestrator::observability::ObservabilityEngine;
using chaos::orchestrator::perturbations::PerturbationEngine;
using chaos::orchestrator::perturbations::PerturbationFactory;
using chaos::orchestrator::validation::validate;

class ManifestE2eTest : public E2eTestBase {};

/**
 * @test Parses a kill manifest, schedules perturbation async, and validates container_running expectation.
 */
TEST_F(ManifestE2eTest, ManifestWithKillRunsToCompletion) {
    std::string json = R"({
        "test_name": "manifest-kill-test",
        "target": {"id": ")" +
                       containerId() + R"("},
        "perturbations": [{"type": "kill", "parameters": {}}],
        "expectations": [{"type": "container_running", "parameters": {}}],
        "duration_s": 3
    })";

    auto manifest = chaos::orchestrator::manifests::ManifestParser::parseFromJson(json);
    EXPECT_EQ(manifest.test_name, "manifest-kill-test");
    EXPECT_EQ(manifest.target.id, containerId());

    PerturbationFactory factory;
    std::vector<std::unique_ptr<chaos::orchestrator::perturbations::IPerturbation>> perturbations;
    perturbations.reserve(manifest.perturbations.size());
    for (const auto& spec : manifest.perturbations) {
        perturbations.push_back(factory.create(engine(), manifest.target, spec));
    }

    PerturbationEngine pert_engine;
    auto duration = std::chrono::seconds(manifest.duration_s.value_or(1));
    pert_engine.scheduleAllAsync(std::move(perturbations), duration);
    pert_engine.waitForTeardown();

    ObservabilityEngine observer(engine());
    const auto state = observer.observe(containerId());

    const auto results = validate(state, manifest.expectations);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

/**
 * @test Parses a memory_cap manifest, schedules perturbation async, and validates container_running expectation.
 */
TEST_F(ManifestE2eTest, ManifestWithMemoryCapRunsToCompletion) {
    std::string json = R"({
        "test_name": "manifest-memory-test",
        "target": {"id": ")" +
                       containerId() + R"("},
        "perturbations": [{"type": "memory_cap", "parameters": {"limit_bytes": "33554432"}}],
        "expectations": [{"type": "container_running", "parameters": {}}],
        "duration_s": 2
    })";

    auto manifest = chaos::orchestrator::manifests::ManifestParser::parseFromJson(json);
    EXPECT_EQ(manifest.test_name, "manifest-memory-test");

    PerturbationFactory factory;
    std::vector<std::unique_ptr<chaos::orchestrator::perturbations::IPerturbation>> perturbations;
    perturbations.reserve(manifest.perturbations.size());
    for (const auto& spec : manifest.perturbations) {
        perturbations.push_back(factory.create(engine(), manifest.target, spec));
    }

    PerturbationEngine pert_engine;
    auto duration = std::chrono::seconds(manifest.duration_s.value_or(1));
    pert_engine.scheduleAllAsync(std::move(perturbations), duration);
    pert_engine.waitForTeardown();

    ObservabilityEngine observer(engine());
    const auto state = observer.observe(containerId());

    const auto results = validate(state, manifest.expectations);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

/**
 * @test Parses a valid JSON manifest without a container fixture and verifies all fields.
 */
TEST(ManifestE2eTest_NoFixture, ParseValidManifest) {
    std::string json = R"({
        "test_name": "simple-test",
        "target": {"id": "abc123"},
        "perturbations": [{"type": "kill", "parameters": {}}],
        "expectations": [{"type": "container_running", "parameters": {}}]
    })";

    auto manifest = chaos::orchestrator::manifests::ManifestParser::parseFromJson(json);
    EXPECT_EQ(manifest.test_name, "simple-test");
    EXPECT_EQ(manifest.target.id, "abc123");
    EXPECT_EQ(manifest.perturbations.size(), 1u);
    EXPECT_EQ(manifest.perturbations[0].type, "kill");
    EXPECT_EQ(manifest.expectations.size(), 1u);
    EXPECT_EQ(manifest.expectations[0].type, "container_running");
}
