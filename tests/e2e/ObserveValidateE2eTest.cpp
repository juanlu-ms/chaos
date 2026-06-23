/**
 * @file ObserveValidateE2eTest.cpp
 * @brief End-to-end tests for observability and validation engines.
 */

#include <gtest/gtest.h>

#include "E2eTestBase.hpp"
#include "core/TargetState.hpp"
#include "manifests/Manifest.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "validation/ValidationEngine.hpp"

using chaos::orchestrator::manifests::Expectation;
using chaos::orchestrator::manifests::Parameters;
using chaos::orchestrator::observability::ObservabilityEngine;
using chaos::orchestrator::validation::validate;

class ObserveValidateE2eTest : public E2eTestBase {};

/**
 * @test Verifies observability engine returns running state with memory info for a running container.
 */
TEST_F(ObserveValidateE2eTest, ObserveReturnsRunningState) {
    ObservabilityEngine observer(engine());
    const auto state = observer.observe(containerId());

    EXPECT_EQ(state.container_id, containerId());
    EXPECT_EQ(state.status, ContainerStatus::Running);
    EXPECT_TRUE(state.memory_usage_mb.has_value());
}

/**
 * @test Validates that a running container passes the container_running expectation.
 */
TEST_F(ObserveValidateE2eTest, ValidateContainerRunningExpectation) {
    ObservabilityEngine observer(engine());
    const auto state = observer.observe(containerId());

    std::vector<Expectation> expectations = {
        {"container_running", Parameters{}},
    };

    const auto results = validate(state, expectations);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
    EXPECT_EQ(results[0].expectation_type, "container_running");
}

/**
 * @test Validates that container_not_running expectation fails for a running container.
 */
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

/**
 * @test Verifies observability engine returns Exited status after stopping a container.
 */
TEST_F(ObserveValidateE2eTest, ObserveStoppedContainerReturnsExitedState) {
    engine()->stopContainer(containerId());

    ObservabilityEngine observer(engine());
    const auto state = observer.observe(containerId());

    EXPECT_EQ(state.status, ContainerStatus::Exited);
}
