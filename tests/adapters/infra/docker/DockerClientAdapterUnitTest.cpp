#include <gtest/gtest.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>
#include <nlohmann/json.hpp>

using chaos::adapters::infra::docker::DockerClientAdapter;
using chaos::adapters::infra::docker::HttpMethod;

namespace {

DockerClientAdapter makeAdapter(DockerClientAdapter::RequestFn fn) { return DockerClientAdapter(std::move(fn)); }

}  // namespace

TEST(DockerClientAdapterUnitTest, ParsesContainerList) {
    auto adapter = makeAdapter([](HttpMethod method, const std::string& endpoint) {
        EXPECT_EQ(method, HttpMethod::GET);
        EXPECT_EQ(endpoint, "/containers/json");
        return nlohmann::json::array({
            {
                {"Id", "abc123"},
                {"Names", nlohmann::json::array({"/mock-container"})},
                {"State", "running"},
            },
        });
    });

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 1U);
    EXPECT_EQ(containers[0].id, "abc123");
    EXPECT_EQ(containers[0].name, "mock-container");
    EXPECT_EQ(containers[0].state, "running");
}

TEST(DockerClientAdapterUnitTest, ParsesEmptyContainerList) {
    auto adapter = makeAdapter([](HttpMethod, const std::string&) { return nlohmann::json::array(); });

    const auto containers = adapter.listContainers();
    EXPECT_TRUE(containers.empty());
}

TEST(DockerClientAdapterUnitTest, ThrowsOnNonArrayResponse) {
    auto adapter =
        makeAdapter([](HttpMethod, const std::string&) { return nlohmann::json::object({{"error", "not an array"}}); });

    EXPECT_THROW(adapter.listContainers(), std::runtime_error);
}

TEST(DockerClientAdapterUnitTest, HandlesContainersWithMissingFields) {
    auto adapter = makeAdapter([](HttpMethod, const std::string&) {
        return nlohmann::json::array({
            {{"Id", "abc123"}},
            {{"Names", nlohmann::json::array({"/only-name"})}},
        });
    });

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 2U);
    EXPECT_EQ(containers[0].id, "abc123");
    EXPECT_TRUE(containers[0].name.empty());
    EXPECT_TRUE(containers[0].state.empty());
    EXPECT_TRUE(containers[1].id.empty());
    EXPECT_EQ(containers[1].name, "only-name");
}

TEST(DockerClientAdapterUnitTest, ParsesMultipleContainers) {
    auto adapter = makeAdapter([](HttpMethod, const std::string&) {
        return nlohmann::json::array({
            {{"Id", "aaa"}, {"Names", nlohmann::json::array({"/alpha"})}, {"State", "running"}},
            {{"Id", "bbb"}, {"Names", nlohmann::json::array({"/beta"})}, {"State", "exited"}},
            {{"Id", "ccc"}, {"Names", nlohmann::json::array({"/gamma"})}, {"State", "paused"}},
        });
    });

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 3U);
    EXPECT_EQ(containers[0].name, "alpha");
    EXPECT_EQ(containers[1].name, "beta");
    EXPECT_EQ(containers[1].state, "exited");
    EXPECT_EQ(containers[2].name, "gamma");
    EXPECT_EQ(containers[2].state, "paused");
}

TEST(DockerClientAdapterUnitTest, PropagatesTransportErrors) {
    auto adapter = makeAdapter(
        [](HttpMethod, const std::string&) -> nlohmann::json { throw std::runtime_error("Connection refused"); });

    EXPECT_THROW(adapter.listContainers(), std::runtime_error);
}
