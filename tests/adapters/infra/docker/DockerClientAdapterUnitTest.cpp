#include <gtest/gtest.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>
#include <nlohmann/json.hpp>

using chaos::adapters::infra::docker::DockerClientAdapter;
using chaos::adapters::infra::docker::HttpMethod;
using chaos::adapters::infra::docker::HttpResponse;

namespace {

// Helper to create an adapter that returns a predefined JSON response.
DockerClientAdapter makeAdapterWithResponse(const nlohmann::json& response) {
    return DockerClientAdapter([response](HttpMethod method, const std::string& endpoint) {
        // We can still add default checks if we want.
        EXPECT_EQ(method, HttpMethod::GET);
        EXPECT_EQ(endpoint, "/containers/json");
        return HttpResponse{200, response.dump()};
    });
}

// Helper to create an adapter that throws a specific error.
DockerClientAdapter makeAdapterWithError(const std::string& error_message) {
    return DockerClientAdapter(
        [error_message](HttpMethod, const std::string&) -> HttpResponse { throw std::runtime_error(error_message); });
}

}  // namespace

TEST(DockerClientAdapterUnitTest, ParsesContainerList) {
    const auto mock_response = nlohmann::json::array({
        {
            {"Id", "abc123"},
            {"Names", nlohmann::json::array({"/mock-container"})},
            {"State", "running"},
        },
    });
    auto adapter = makeAdapterWithResponse(mock_response);

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 1U);
    EXPECT_EQ(containers[0].id, "abc123");
    EXPECT_EQ(containers[0].name, "mock-container");
    EXPECT_EQ(containers[0].state, "running");
}

TEST(DockerClientAdapterUnitTest, ParsesEmptyContainerList) {
    auto adapter = makeAdapterWithResponse(nlohmann::json::array());

    const auto containers = adapter.listContainers();
    EXPECT_TRUE(containers.empty());
}

TEST(DockerClientAdapterUnitTest, ThrowsOnNonArrayResponse) {
    auto adapter = makeAdapterWithResponse(nlohmann::json::object({{"error", "not an array"}}));

    EXPECT_THROW(adapter.listContainers(), std::runtime_error);
}

TEST(DockerClientAdapterUnitTest, HandlesContainersWithMissingFields) {
    const auto mock_response = nlohmann::json::array({
        {{"Id", "abc123"}},
        {{"Names", nlohmann::json::array({"/only-name"})}},
    });
    auto adapter = makeAdapterWithResponse(mock_response);

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 2U);
    EXPECT_EQ(containers[0].id, "abc123");
    EXPECT_TRUE(containers[0].name.empty());
    EXPECT_TRUE(containers[0].state.empty());
    EXPECT_TRUE(containers[1].id.empty());
    EXPECT_EQ(containers[1].name, "only-name");
}

TEST(DockerClientAdapterUnitTest, ParsesMultipleContainers) {
    const auto mock_response = nlohmann::json::array({
        {{"Id", "aaa"}, {"Names", nlohmann::json::array({"/alpha"})}, {"State", "running"}},
        {{"Id", "bbb"}, {"Names", nlohmann::json::array({"/beta"})}, {"State", "exited"}},
        {{"Id", "ccc"}, {"Names", nlohmann::json::array({"/gamma"})}, {"State", "paused"}},
    });
    auto adapter = makeAdapterWithResponse(mock_response);

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 3U);
    EXPECT_EQ(containers[0].name, "alpha");
    EXPECT_EQ(containers[1].name, "beta");
    EXPECT_EQ(containers[1].state, "exited");
    EXPECT_EQ(containers[2].name, "gamma");
    EXPECT_EQ(containers[2].state, "paused");
}

TEST(DockerClientAdapterUnitTest, PropagatesTransportErrors) {
    auto adapter = makeAdapterWithError("Connection refused");

    EXPECT_THROW(adapter.listContainers(), std::runtime_error);
}

TEST(DockerClientAdapterUnitTest, KillContainerCallsCorrectEndpoint) {
    bool was_called = false;
    const std::string container_id = "test-container-123";

    auto adapter = DockerClientAdapter([&was_called, &container_id](HttpMethod method, const std::string& endpoint) {
        was_called = true;
        EXPECT_EQ(method, HttpMethod::POST);
        EXPECT_EQ(endpoint, "/containers/" + container_id + "/kill");
        return HttpResponse{204, ""};
    });

    adapter.killContainer(container_id);
    EXPECT_TRUE(was_called);
}

TEST(DockerClientAdapterUnitTest, KillContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon is on fire");

    EXPECT_THROW(adapter.killContainer("any-id"), std::runtime_error);
}
