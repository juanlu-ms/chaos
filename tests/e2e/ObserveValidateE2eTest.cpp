#include <gtest/gtest.h>

#include "E2eTestBase.hpp"
#include "manifests/Manifest.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "shared/TargetState.hpp"
#include "validation/ValidationEngine.hpp"

using chaos::orchestrator::manifests::Expectation;
using chaos::orchestrator::manifests::Parameters;
using chaos::orchestrator::observability::ObservabilityEngine;
using chaos::orchestrator::validation::validate;

class ObserveValidateE2eTest : public E2eTestBase {};

TEST_F(ObserveValidateE2eTest, ObserveReturnsRunningState) {
    ObservabilityEngine observer(engine());
    const auto state = observer.observe(containerId());

    EXPECT_EQ(state.container_id, containerId());
    EXPECT_EQ(state.status, ContainerStatus::Running);
    EXPECT_TRUE(state.memory_usage_mb.has_value());
}

TEST_F(ObserveValidateE2eTest, ValidateContainerRunningExpectation) {
    ObservabilityEngine observer(engine());
    const auto state = observer.observe(containerId());

    std::vector<Expectation> expectations = {
        {"container_running", Parameters{}},
    };

    const auto results = validate(state, expectations);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
    EXPECT_EQ(results[0].expectationType, "container_running");
}

TEST_F(ObserveValidateE2eTest, ValidateContainerNotRunningFails) {
    ObservabilityEngine observer(engine());
    const auto state = observer.observe(containerId());

    std::vector<Expectation> expectations = {
        {"container_not_running", Parameters{}},
    };

    const auto results = validate(state, expectations);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

TEST_F(ObserveValidateE2eTest, ObserveStoppedContainerReturnsExitedState) {
    engine()->stopContainer(containerId());

    ObservabilityEngine observer(engine());
    const auto state = observer.observe(containerId());

    EXPECT_EQ(state.status, ContainerStatus::Exited);
}
