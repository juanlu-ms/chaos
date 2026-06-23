#include <gtest/gtest.h>

#include "ContainerSmokeTestBase.hpp"

/**
 * @file DockerClientIntegrationTest.cpp
 * @brief Smoke tests for Docker engine integration in real environments.
 */

using namespace chaos::orchestrator::tests::smoke;

/**
 * @test Verifies container listing when Docker is available.
 */
class DockerClientIntegrationTest : public ContainerSmokeTestBase {};

TEST_F(DockerClientIntegrationTest, ListsContainersWhenDockerAvailable) {
    createAndStartContainer();

    const auto containers = engine_->listContainers();
    ASSERT_GE(containers.size(), 1u);
    bool foundCreated = false;
    for (const auto& container : containers) {
        EXPECT_FALSE(container.id.empty());
        if (containerId_.find(container.id) == 0 || container.id.find(containerId_) == 0) foundCreated = true;
    }
    EXPECT_TRUE(foundCreated) << "Created container not found in listing";
}

/**
 * @test Verifies getSystemInfo returns a positive mem_total value.
 */
TEST(DockerClientSystemInfoTest, GetSystemInfoReturnsTotalMemory) {
    auto adapter = createTestEngine();
    const auto info = adapter->getSystemInfo();
    EXPECT_GT(info.mem_total, 0);
}
