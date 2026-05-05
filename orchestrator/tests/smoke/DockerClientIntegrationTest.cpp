#include <gtest/gtest.h>

#include "containers/ContainerEngineFactory.hpp"

/**
 * @file DockerClientIntegrationTest.cpp
 * @brief Smoke tests for Docker engine integration in real environments.
 */

using chaos::orchestrator::containers::createContainerEngine;

/**
 * @test Verifies container listing when Docker is available.
 */
TEST(DockerClientIntegrationTest, ListsContainersWhenDockerAvailable) {
    auto adapter = createContainerEngine();
    const auto testId = adapter->createContainer("chaos-demo-target:latest", {});
    ASSERT_FALSE(testId.empty());
    adapter->startContainer(testId);

    const auto containers = adapter->listContainers();
    ASSERT_GE(containers.size(), 1u);
    bool foundCreated = false;
    for (const auto& container : containers) {
        EXPECT_FALSE(container.id.empty());
        if (testId.find(container.id) == 0 || container.id.find(testId) == 0) foundCreated = true;
    }
    EXPECT_TRUE(foundCreated) << "Created container not found in listing";

    adapter->removeContainer(testId);
}

TEST(DockerClientIntegrationTest, GetSystemInfoReturnsTotalMemory) {
    auto adapter = createContainerEngine();
    const auto info = adapter->getSystemInfo();
    EXPECT_GT(info.memTotal, 0);
}
