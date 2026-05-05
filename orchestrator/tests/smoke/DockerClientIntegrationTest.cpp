#include <gtest/gtest.h>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"

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
    const auto containers = adapter->listContainers();
    for (const auto& container : containers) {
        EXPECT_FALSE(container.id.empty());
    }
}

TEST(DockerClientIntegrationTest, GetSystemInfoReturnsTotalMemory) {
    auto adapter = createContainerEngine();
    const auto info = adapter->getSystemInfo();
    EXPECT_GT(info.memTotal, 0);
}
