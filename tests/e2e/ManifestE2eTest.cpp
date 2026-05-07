#include <gtest/gtest.h>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "validation/ValidationEngine.hpp"

using chaos::orchestrator::containers::createContainerEngine;
using chaos::orchestrator::containers::IContainerEngine;
using chaos::orchestrator::manifests::Expectation;
using chaos::orchestrator::manifests::Parameters;
using chaos::orchestrator::observability::ObservabilityEngine;
using chaos::orchestrator::perturbations::PerturbationEngine;
using chaos::orchestrator::perturbations::PerturbationFactory;
using chaos::orchestrator::validation::ValidationEngine;

class ManifestE2eTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = createContainerEngine();
        engine_->buildImage("chaos-demo-target:latest", CHAOS_EXAMPLES_DIR "/demo-target/Dockerfile");
        containerId_ = engine_->createContainer("chaos-demo-target:latest", {});
        ASSERT_FALSE(containerId_.empty());
        engine_->startContainer(containerId_);
    }

    void TearDown() override {
        if (engine_ && !containerId_.empty()) {
            try {
                engine_->removeContainer(containerId_);
            } catch (const std::exception& ex) {
                ADD_FAILURE() << "Failed to remove container " << containerId_ << " during teardown: " << ex.what();
            }
            containerId_.clear();
        }
    }

    std::shared_ptr<IContainerEngine> engine_;
    std::string containerId_;
};

TEST_F(ManifestE2eTest, ManifestWithKillRunsToCompletion) {
    std::string json = R"({
        "test_name": "manifest-kill-test",
        "target": {"id": ")" +
                       containerId_ + R"("},
        "perturbations": [{"type": "kill", "parameters": {}}],
        "expectations": [{"type": "container_running", "parameters": {}}],
        "duration_s": 3
    })";

    auto manifest = chaos::orchestrator::manifests::ManifestParser::parseFromJson(json);
    EXPECT_EQ(manifest.test_name, "manifest-kill-test");
    EXPECT_EQ(manifest.target.id, containerId_);

    PerturbationFactory factory;
    std::vector<std::unique_ptr<chaos::orchestrator::perturbations::IPerturbation>> perturbations;
    perturbations.reserve(manifest.perturbations.size());
    for (const auto& spec : manifest.perturbations) {
        perturbations.push_back(factory.create(engine_, manifest.target, spec));
    }

    PerturbationEngine pertEngine;
    auto duration = std::chrono::seconds(manifest.duration_s.value_or(1));
    pertEngine.scheduleAllAsync(std::move(perturbations), duration);
    pertEngine.waitForTeardown();

    ObservabilityEngine observer(engine_);
    const auto state = observer.observe(containerId_);

    const auto results = ValidationEngine::validate(state, manifest.expectations);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

TEST_F(ManifestE2eTest, ManifestWithMemoryCapRunsToCompletion) {
    std::string json = R"({
        "test_name": "manifest-memory-test",
        "target": {"id": ")" +
                       containerId_ + R"("},
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
        perturbations.push_back(factory.create(engine_, manifest.target, spec));
    }

    PerturbationEngine pertEngine;
    auto duration = std::chrono::seconds(manifest.duration_s.value_or(1));
    pertEngine.scheduleAllAsync(std::move(perturbations), duration);
    pertEngine.waitForTeardown();

    ObservabilityEngine observer(engine_);
    const auto state = observer.observe(containerId_);

    const auto results = ValidationEngine::validate(state, manifest.expectations);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

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
