#include <gtest/gtest.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>

using chaos::adapters::infra::docker::DockerClientAdapter;

TEST(DockerClientAdapterTest, RetrievesRunningContainers) {
    auto adapter = DockerClientAdapter::create();

    const auto containers = adapter.listContainers();

    ASSERT_FALSE(containers.empty());
}
