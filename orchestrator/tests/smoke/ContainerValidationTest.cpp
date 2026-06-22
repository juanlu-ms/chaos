/**
 * @file ContainerValidationTest.cpp
 * @brief Smoke tests for input validation of container engine methods.
 */

#include <gtest/gtest.h>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"

using chaos::orchestrator::containers::createContainerEngine;

/** @brief Test fixture for input validation smoke tests of container engine methods. */
class ContainerValidationTest : public ::testing::Test {
protected:
    void SetUp() override { engine_ = createContainerEngine(); }

    std::shared_ptr<chaos::orchestrator::containers::IContainerEngine> engine_;
};

/**
 * @test Verifies startContainer throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, StartContainerThrowsOnEmptyId) {
    EXPECT_THROW(engine_->startContainer(""), std::invalid_argument);
}

/**
 * @test Verifies stopContainer throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, StopContainerThrowsOnEmptyId) {
    EXPECT_THROW(engine_->stopContainer(""), std::invalid_argument);
}

/**
 * @test Verifies killContainer throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, KillContainerThrowsOnEmptyId) {
    EXPECT_THROW(engine_->killContainer(""), std::invalid_argument);
}

/**
 * @test Verifies removeContainer throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, RemoveContainerThrowsOnEmptyId) {
    EXPECT_THROW(engine_->removeContainer(""), std::invalid_argument);
}

/**
 * @test Verifies getStatus throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, GetStatusThrowsOnEmptyId) {
    EXPECT_THROW((void)engine_->getStatus(""), std::invalid_argument);
}

/**
 * @test Verifies exec throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, ExecThrowsOnEmptyContainerId) {
    EXPECT_THROW((void)engine_->exec("", "echo hello"), std::invalid_argument);
}

/**
 * @test Verifies exec throws std::invalid_argument for empty command.
 */
TEST_F(ContainerValidationTest, ExecThrowsOnEmptyCommand) {
    EXPECT_THROW((void)engine_->exec("some-id", ""), std::invalid_argument);
}

/**
 * @test Verifies getLogs throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, GetLogsThrowsOnEmptyContainerId) {
    EXPECT_THROW((void)engine_->getLogs(""), std::invalid_argument);
}

/**
 * @test Verifies updateMemoryLimit throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, UpdateMemoryLimitThrowsOnEmptyId) {
    EXPECT_THROW((void)engine_->updateMemoryLimit("", 1024), std::invalid_argument);
}

/**
 * @test Verifies updateCpuQuota throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, UpdateCpuQuotaThrowsOnEmptyId) {
    EXPECT_THROW((void)engine_->updateCpuQuota("", 50000, 100000), std::invalid_argument);
}

/**
 * @test Verifies getStats throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, GetContainerMemoryUsageThrowsOnEmptyId) {
    EXPECT_THROW((void)engine_->getStats(""), std::invalid_argument);
}

/**
 * @test Verifies getStats throws std::invalid_argument for empty container ID (CPU variant).
 */
TEST_F(ContainerValidationTest, GetContainerCpuUsageThrowsOnEmptyId) {
    EXPECT_THROW((void)engine_->getStats(""), std::invalid_argument);
}

/**
 * @test Verifies getContainerIp throws std::invalid_argument for empty container ID.
 */
TEST_F(ContainerValidationTest, GetContainerIpThrowsOnEmptyId) {
    EXPECT_THROW((void)engine_->getContainerIp(""), std::invalid_argument);
}
