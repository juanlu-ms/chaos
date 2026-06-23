/**
 * @file ContainerStateValidationUnitTest.cpp
 * @brief Unit tests for ContainerRunningValidation and ContainerNotRunningValidation.
 */

#include <gtest/gtest.h>

#include "core/TargetState.hpp"
#include "manifests/Manifest.hpp"
#include "validation/internal/ContainerStateValidation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::validation;

namespace {

core::TargetState makeState(containers::ContainerStatus status) {
    core::TargetState s;
    s.container_id = "test-ctr";
    s.status = status;
    return s;
}

}  // namespace

/**
 * @test Verifies ContainerRunningValidation passes when status is Running.
 */
TEST(ContainerStateValidationTest, RunningPassesWhenRunning) {
    ContainerRunningValidation v;
    auto result = v.validate(makeState(containers::ContainerStatus::Running), {"container_running", {}});
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.expectation_type, "container_running");
}

/**
 * @test Verifies ContainerRunningValidation fails when status is Exited.
 */
TEST(ContainerStateValidationTest, RunningFailsWhenExited) {
    ContainerRunningValidation v;
    auto result = v.validate(makeState(containers::ContainerStatus::Exited), {"container_running", {}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies ContainerRunningValidation fails when status is Dead.
 */
TEST(ContainerStateValidationTest, RunningFailsWhenDead) {
    ContainerRunningValidation v;
    auto result = v.validate(makeState(containers::ContainerStatus::Dead), {"container_running", {}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies ContainerRunningValidation fails when status is Unknown.
 */
TEST(ContainerStateValidationTest, RunningFailsWhenUnknown) {
    ContainerRunningValidation v;
    auto result = v.validate(makeState(containers::ContainerStatus::Unknown), {"container_running", {}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies ContainerNotRunningValidation passes when status is Exited.
 */
TEST(ContainerStateValidationTest, NotRunningPassesWhenExited) {
    ContainerNotRunningValidation v;
    auto result = v.validate(makeState(containers::ContainerStatus::Exited), {"container_not_running", {}});
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.expectation_type, "container_not_running");
}

/**
 * @test Verifies ContainerNotRunningValidation passes when status is Dead.
 */
TEST(ContainerStateValidationTest, NotRunningPassesWhenDead) {
    ContainerNotRunningValidation v;
    auto result = v.validate(makeState(containers::ContainerStatus::Dead), {"container_not_running", {}});
    EXPECT_TRUE(result.passed);
}

/**
 * @test Verifies ContainerNotRunningValidation fails when status is Running.
 */
TEST(ContainerStateValidationTest, NotRunningFailsWhenRunning) {
    ContainerNotRunningValidation v;
    auto result = v.validate(makeState(containers::ContainerStatus::Running), {"container_not_running", {}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies ContainerNotRunningValidation handles Unknown status gracefully (Unknown is not Running, so passes).
 */
TEST(ContainerStateValidationTest, NotRunningHandlesUnknown) {
    ContainerNotRunningValidation v;
    auto result = v.validate(makeState(containers::ContainerStatus::Unknown), {"container_not_running", {}});
    EXPECT_TRUE(result.passed);
}
