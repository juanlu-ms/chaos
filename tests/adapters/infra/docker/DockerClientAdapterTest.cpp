#include <gtest/gtest.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>

using chaos::adapters::infra::docker::DockerClientAdapter;

TEST(DockerClientAdapterIntegrationTest, ListsContainersWhenDockerAvailable) {
    auto adapter = DockerClientAdapter::create();

    try {
        const auto containers = adapter->listContainers();
        for (const auto& container : containers) {
            EXPECT_FALSE(container.id.empty());
        }
    } catch (const std::exception& ex) {
        GTEST_SKIP() << "Docker socket unavailable: " << ex.what();
    }
}
