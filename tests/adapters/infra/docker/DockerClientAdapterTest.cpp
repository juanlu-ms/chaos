#include <gtest/gtest.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>

using kaos::adapters::infra::docker::DockerClientAdapter;

TEST(DockerClientAdapterTest, RetrievesRunningContainers) {
    auto result = DockerClientAdapter::create();
    ASSERT_TRUE(result.has_value()) << result.error();
    auto adapter = std::move(result.value());

    const auto containers = adapter.listContainers();

    ASSERT_FALSE(containers.empty());
}
