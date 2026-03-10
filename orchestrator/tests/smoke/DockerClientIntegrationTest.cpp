#include <gtest/gtest.h>

#include <containers/internal/DockerClient.hpp>

using chaos::orchestrator::containers::internal::DockerClient;

TEST(DockerClientIntegrationTest, ListsContainersWhenDockerAvailable) {
    auto adapter = DockerClient::create();

    try {
        const auto containers = adapter->listContainers();
        for (const auto& container : containers) {
            EXPECT_FALSE(container.id.empty());
        }
    } catch (const std::exception& ex) {
        GTEST_SKIP() << "Docker socket unavailable: " << ex.what();
    }
}
