#include <gtest/gtest.h>

#include <containers/ContainerEngineFactory.hpp>
#include <containers/IContainerEngine.hpp>

/**
 * @file DockerClientIntegrationTest.cpp
 * @brief Smoke test for Docker engine integration in real environments.
 */

using chaos::orchestrator::containers::createContainerEngine;

/**
 * @test Verifies container listing when Docker is available.
 */
TEST(DockerClientIntegrationTest, ListsContainersWhenDockerAvailable) {
    auto adapter = createContainerEngine();

    try {
        const auto containers = adapter->listContainers();
        for (const auto& container : containers) {
            EXPECT_FALSE(container.id.empty());
        }
    } catch (const chaos::orchestrator::containers::ContainerEngineError& ex) {
        GTEST_SKIP() << "Docker socket unavailable: " << ex.what();
    }
}
