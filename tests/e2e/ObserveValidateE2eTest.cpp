#include <gtest/gtest.h>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "shared/ContainerStatus.hpp"
#include "shared/TargetState.hpp"
#include "validation/ValidationEngine.hpp"

using chaos::orchestrator::containers::createContainerEngine;
using chaos::orchestrator::containers::IContainerEngine;
using chaos::orchestrator::manifests::Expectation;
using chaos::orchestrator::manifests::Parameters;
using chaos::orchestrator::observability::ObservabilityEngine;
using chaos::orchestrator::shared::ContainerStatus;
using chaos::orchestrator::validation::validate;

class ObserveValidateE2eTest : public ::testing::Test {
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

TEST_F(ObserveValidateE2eTest, ObserveReturnsRunningState) {
    ObservabilityEngine observer(engine_);
    const auto state = observer.observe(containerId_);

    EXPECT_EQ(state.container_id, containerId_);
    EXPECT_EQ(state.status, ContainerStatus::Running);
    EXPECT_TRUE(state.memory_usage_mb.has_value());
}

TEST_F(ObserveValidateE2eTest, ValidateContainerRunningExpectation) {
    ObservabilityEngine observer(engine_);
    const auto state = observer.observe(containerId_);

    std::vector<Expectation> expectations = {
        {"container_running", Parameters{}},
    };

    const auto results = validate(state, expectations);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
    EXPECT_EQ(results[0].expectationType, "container_running");
}

TEST_F(ObserveValidateE2eTest, ValidateContainerNotRunningFails) {
    ObservabilityEngine observer(engine_);
    const auto state = observer.observe(containerId_);

    std::vector<Expectation> expectations = {
        {"container_not_running", Parameters{}},
    };

    const auto results = validate(state, expectations);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

TEST_F(ObserveValidateE2eTest, ObserveStoppedContainerReturnsExitedState) {
    engine_->stopContainer(containerId_);

    ObservabilityEngine observer(engine_);
    const auto state = observer.observe(containerId_);

    EXPECT_EQ(state.status, ContainerStatus::Exited);
}
