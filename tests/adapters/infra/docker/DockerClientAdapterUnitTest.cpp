#include <fmt/format.h>
#include <gtest/gtest.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>
#include <nlohmann/json.hpp>
#include <string_view>

using chaos::adapters::infra::docker::DockerClientAdapter;
using chaos::adapters::infra::docker::HttpMethod;
using chaos::adapters::infra::docker::HttpResponse;

namespace {

// Helper to create an adapter that returns a predefined JSON response.
DockerClientAdapter makeAdapterForListContainers(const nlohmann::json& response) {
    return DockerClientAdapter([response](HttpMethod method, std::string_view endpoint) {
        // We can still add default checks if we want.
        EXPECT_EQ(method, HttpMethod::GET);
        EXPECT_EQ(endpoint, "/containers/json");
        return HttpResponse{200, response.dump()};
    });
}

// Helper to create an adapter that throws a specific error.
DockerClientAdapter makeAdapterWithError(const std::string& error_message) {
    return DockerClientAdapter(
        [error_message](HttpMethod, std::string_view) -> HttpResponse { throw std::runtime_error(error_message); });
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
    auto adapter = makeAdapterForListContainers(mock_response);

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 1U);
    EXPECT_EQ(containers[0].id, "abc123");
    EXPECT_EQ(containers[0].name, "mock-container");
    EXPECT_EQ(containers[0].state, "running");
}

TEST(DockerClientAdapterUnitTest, ParsesEmptyContainerList) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    const auto containers = adapter.listContainers();
    EXPECT_TRUE(containers.empty());
}

TEST(DockerClientAdapterUnitTest, ThrowsOnNonArrayResponse) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::object({{"error", "not an array"}}));

    EXPECT_THROW(adapter.listContainers(), std::runtime_error);
}

TEST(DockerClientAdapterUnitTest, HandlesContainersWithMissingFields) {
    const auto mock_response = nlohmann::json::array({
        {{"Id", "abc123"}},
        {{"Names", nlohmann::json::array({"/only-name"})}},
    });
    auto adapter = makeAdapterForListContainers(mock_response);

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
    auto adapter = makeAdapterForListContainers(mock_response);

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

TEST(DockerClientAdapterUnitTest, StopContainerCallsCorrectEndpoint) {
    bool was_called = false;
    const std::string container_id = "test-container-456";

    auto adapter = DockerClientAdapter([&was_called, &container_id](HttpMethod method, std::string_view endpoint) {
        was_called = true;
        EXPECT_EQ(method, HttpMethod::POST);
        EXPECT_EQ(endpoint, fmt::format("/containers/{}/stop?t=5", container_id));
        return HttpResponse{204, ""};
    });

    adapter.stopContainer(container_id);
    EXPECT_TRUE(was_called);
}

TEST(DockerClientAdapterUnitTest, StopContainerWithCustomTimeout) {
    const std::string container_id = "test-container-789";
    bool endpoint_correct = false;

    auto adapter =
        DockerClientAdapter([&endpoint_correct, &container_id](HttpMethod method, std::string_view endpoint) {
            EXPECT_EQ(method, HttpMethod::POST);
            endpoint_correct = endpoint == fmt::format("/containers/{}/stop?t=5", container_id);
            return HttpResponse{204, ""};
        });

    adapter.stopContainer(container_id);
    EXPECT_TRUE(endpoint_correct);
}

TEST(DockerClientAdapterUnitTest, StopContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon unreachable");

    EXPECT_THROW(adapter.stopContainer("any-id"), std::runtime_error);
}

TEST(DockerClientAdapterUnitTest, StopContainerWithEmptyIdThrows) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW(adapter.stopContainer(""), std::runtime_error);
}

TEST(DockerClientAdapterUnitTest, KillContainerCallsCorrectEndpoint) {
    bool was_called = false;
    const std::string container_id = "test-container-123";

    auto adapter = DockerClientAdapter([&was_called, &container_id](HttpMethod method, std::string_view endpoint) {
        was_called = true;
        EXPECT_EQ(method, HttpMethod::POST);
        EXPECT_EQ(endpoint, fmt::format("/containers/{}/kill", container_id));
        return HttpResponse{204, ""};
    });

    adapter.killContainer(container_id);
    EXPECT_TRUE(was_called);
}

TEST(DockerClientAdapterUnitTest, KillContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon is on fire");

    EXPECT_THROW(adapter.killContainer("any-id"), std::runtime_error);
}
