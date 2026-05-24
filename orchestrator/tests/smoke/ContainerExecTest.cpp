/**
 * @file ContainerExecTest.cpp
 * @brief Smoke tests for container exec and log retrieval operations.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "ContainerSmokeTestBase.hpp"

using namespace chaos::orchestrator::tests::smoke;

class ContainerExecTest : public ContainerSmokeTestBase {
protected:
    void SetUp() override {
        ContainerSmokeTestBase::SetUp();
        createAndStartContainer();
    }
};

/**
 * @test Verifies exec returns the output of a simple echo command.
 */
TEST_F(ContainerExecTest, ExecSimpleCommandReturnsOutput) {
    const auto output = engine_->exec(containerId_, "echo hello");
    EXPECT_TRUE(output.find("hello") != std::string::npos);
}

/**
 * @test Verifies exec returns non-empty output for the whoami command.
 */
TEST_F(ContainerExecTest, ExecWithComplexCommand) {
    const auto output = engine_->exec(containerId_, "whoami");
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("root"), std::string::npos);
}

/**
 * @test Verifies getLogs returns non-empty content after command execution.
 */
TEST_F(ContainerExecTest, GetLogsReturnsNonEmpty) {
    (void)engine_->exec(containerId_, "echo log_test_marker_42");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto logs = engine_->getLogs(containerId_);
    EXPECT_FALSE(logs.empty());
}
