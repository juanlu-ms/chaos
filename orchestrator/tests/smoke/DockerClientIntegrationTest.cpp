#include <gtest/gtest.h>

#include <containers/ContainerEngineFactory.hpp>

using chaos::orchestrator::containers::createContainerEngine;

TEST(DockerClientIntegrationTest, ListsContainersWhenDockerAvailable) {
    auto adapter = createContainerEngine();

    try {
        const auto containers = adapter->listContainers();
        for (const auto& container : containers) {
            EXPECT_FALSE(container.id.empty());
        }
    } catch (const std::exception& ex) {
        GTEST_SKIP() << "Docker socket unavailable: " << ex.what();
    }
}
