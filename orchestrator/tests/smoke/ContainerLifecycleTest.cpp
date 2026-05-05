#include <gtest/gtest.h>

#include "ContainerSmokeTestBase.hpp"
#include "shared/ContainerStatus.hpp"

using chaos::orchestrator::shared::ContainerStatus;
using namespace chaos::orchestrator::tests::smoke;

class ContainerLifecycleTest : public ContainerSmokeTestBase {};

TEST_F(ContainerLifecycleTest, CreateContainerReturnsNonEmptyId) {
    containerId_ = engine_->createContainer("chaos-demo-target:latest", {});
    EXPECT_FALSE(containerId_.empty());
}

TEST_F(ContainerLifecycleTest, CreateContainerWithEnvOptions) {
    containerId_ = engine_->createContainer("chaos-demo-target:latest", {"FOO=bar", "BAZ=qux"});
    EXPECT_FALSE(containerId_.empty());
}

TEST_F(ContainerLifecycleTest, StartContainerAndGetRunningStatus) {
    createAndStartContainer();
    const auto status = engine_->getStatus(containerId_);
    EXPECT_EQ(status, ContainerStatus::Running);
}

TEST_F(ContainerLifecycleTest, StopContainerAndGetExitedStatus) {
    createAndStartContainer();
    engine_->stopContainer(containerId_);
    const auto status = engine_->getStatus(containerId_);
    EXPECT_EQ(status, ContainerStatus::Exited);
}

TEST_F(ContainerLifecycleTest, KillContainerAndGetExitedStatus) {
    createAndStartContainer();
    engine_->killContainer(containerId_);
    const auto status = engine_->getStatus(containerId_);
    EXPECT_TRUE(status == ContainerStatus::Exited || status == ContainerStatus::Dead);
}

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
