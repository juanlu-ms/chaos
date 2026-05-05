#include <gtest/gtest.h>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"

using chaos::orchestrator::containers::createContainerEngine;

class ContainerValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = createContainerEngine();
    }

    std::shared_ptr<chaos::orchestrator::containers::IContainerEngine> engine_;
};

TEST_F(ContainerValidationTest, StartContainerThrowsOnEmptyId) {
    EXPECT_THROW(engine_->startContainer(""), std::invalid_argument);
}

TEST_F(ContainerValidationTest, StopContainerThrowsOnEmptyId) {
    EXPECT_THROW(engine_->stopContainer(""), std::invalid_argument);
}

TEST_F(ContainerValidationTest, KillContainerThrowsOnEmptyId) {
    EXPECT_THROW(engine_->killContainer(""), std::invalid_argument);
}

TEST_F(ContainerValidationTest, RemoveContainerThrowsOnEmptyId) {
    EXPECT_THROW(engine_->removeContainer(""), std::invalid_argument);
}

TEST_F(ContainerValidationTest, GetStatusThrowsOnEmptyId) {
    EXPECT_THROW(engine_->getStatus(""), std::invalid_argument);
}

TEST_F(ContainerValidationTest, ExecThrowsOnEmptyContainerId) {
    EXPECT_THROW(engine_->exec("", "echo hello"), std::invalid_argument);
}

TEST_F(ContainerValidationTest, ExecThrowsOnEmptyCommand) {
    EXPECT_THROW(engine_->exec("some-id", ""), std::invalid_argument);
}

TEST_F(ContainerValidationTest, GetLogsThrowsOnEmptyContainerId) {
    EXPECT_THROW(engine_->getLogs(""), std::invalid_argument);
}

TEST_F(ContainerValidationTest, UpdateMemoryLimitThrowsOnEmptyId) {
    EXPECT_THROW(engine_->updateMemoryLimit("", 1024), std::invalid_argument);
}

TEST_F(ContainerValidationTest, UpdateCpuQuotaThrowsOnEmptyId) {
    EXPECT_THROW(engine_->updateCpuQuota("", 50000, 100000), std::invalid_argument);
}

TEST_F(ContainerValidationTest, GetContainerMemoryUsageThrowsOnEmptyId) {
    EXPECT_THROW(engine_->getContainerMemoryUsage(""), std::invalid_argument);
}

TEST_F(ContainerValidationTest, GetContainerCpuUsageThrowsOnEmptyId) {
    EXPECT_THROW(engine_->getContainerCpuUsage(""), std::invalid_argument);
}

TEST_F(ContainerValidationTest, GetContainerIpThrowsOnEmptyId) {
    EXPECT_THROW(engine_->getContainerIp(""), std::invalid_argument);
}
