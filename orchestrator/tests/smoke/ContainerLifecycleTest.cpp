/**
 * @file ContainerLifecycleTest.cpp
 * @brief Smoke tests for container create/start/stop/kill/remove lifecycle.
 */

#include <gtest/gtest.h>

#include "ContainerSmokeTestBase.hpp"
#include "shared/ContainerStatus.hpp"

using chaos::orchestrator::shared::ContainerStatus;
using namespace chaos::orchestrator::tests::smoke;

class ContainerLifecycleTest : public ContainerSmokeTestBase {};

/**
 * @test Verifies createContainer returns a non-empty container ID.
 */
TEST_F(ContainerLifecycleTest, CreateContainerReturnsNonEmptyId) {
    containerId_ = engine_->createContainer("chaos-demo-target:latest", {});
    EXPECT_FALSE(containerId_.empty());
}

/**
 * @test Verifies environment variables are set in the created container.
 */
TEST_F(ContainerLifecycleTest, CreateContainerWithEnvOptions) {
    containerId_ = engine_->createContainer("chaos-demo-target:latest", {"FOO=bar", "BAZ=qux"});
    ASSERT_FALSE(containerId_.empty());

    engine_->startContainer(containerId_);
    const auto output = engine_->exec(containerId_, "echo $FOO");
    EXPECT_TRUE(output.find("bar") != std::string::npos);
}

/**
 * @test Verifies a started container reports Running status.
 */
TEST_F(ContainerLifecycleTest, StartContainerAndGetRunningStatus) {
    createAndStartContainer();
    const auto status = engine_->getStatus(containerId_);
    EXPECT_EQ(status, ContainerStatus::Running);
}

/**
 * @test Verifies a stopped container reports Exited status.
 */
TEST_F(ContainerLifecycleTest, StopContainerAndGetExitedStatus) {
    createAndStartContainer();
    engine_->stopContainer(containerId_);
    const auto status = engine_->getStatus(containerId_);
    EXPECT_EQ(status, ContainerStatus::Exited);
}

/**
 * @test Verifies a killed container reports Exited or Dead status.
 */
TEST_F(ContainerLifecycleTest, KillContainerAndGetExitedStatus) {
    createAndStartContainer();
    engine_->killContainer(containerId_);
    const auto status = engine_->getStatus(containerId_);
    EXPECT_TRUE(status == ContainerStatus::Exited || status == ContainerStatus::Dead);
}

/**
 * @test Verifies a removed container no longer appears in container listings.
 */
TEST_F(ContainerLifecycleTest, RemoveContainer) {
    containerId_ = engine_->createContainer("chaos-demo-target:latest", {});
    ASSERT_FALSE(containerId_.empty());
    EXPECT_NO_THROW(engine_->removeContainer(containerId_));
    const auto containers = engine_->listContainers();
    for (const auto& c : containers) {
        EXPECT_NE(c.id, containerId_);
    }
    containerId_.clear();
}

/**
 * @test Verifies the full create/start/stop/start/kill/remove lifecycle sequence.
 */
TEST_F(ContainerLifecycleTest, LifecycleFullCycle) {
    containerId_ = engine_->createContainer("chaos-demo-target:latest", {});
    ASSERT_FALSE(containerId_.empty());

    engine_->startContainer(containerId_);
    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);

    engine_->stopContainer(containerId_);
    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Exited);

    engine_->startContainer(containerId_);
    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);

    engine_->killContainer(containerId_);
    const auto finalStatus = engine_->getStatus(containerId_);
    EXPECT_TRUE(finalStatus == ContainerStatus::Exited || finalStatus == ContainerStatus::Dead);

    engine_->removeContainer(containerId_);
    containerId_.clear();
}
