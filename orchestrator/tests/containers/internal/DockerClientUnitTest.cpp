#include <fmt/format.h>
#include <gtest/gtest.h>

#include <containers/internal/DockerClient.hpp>
#include <nlohmann/json.hpp>
#include <string_view>

using chaos::orchestrator::containers::internal::DockerClient;
using chaos::orchestrator::containers::internal::HttpMethod;
using chaos::orchestrator::containers::internal::HttpResponse;

namespace {

DockerClient makeAdapterForListContainers(const nlohmann::json& response) {
    return DockerClient([response](HttpMethod method, std::string_view endpoint) {
        EXPECT_EQ(method, HttpMethod::GET);
        EXPECT_EQ(endpoint, "/containers/json");
        return HttpResponse{200, response.dump()};
    });
}

DockerClient makeAdapterWithError(const std::string& errorMessage) {
    return DockerClient(
        [errorMessage](HttpMethod, std::string_view) -> HttpResponse { throw std::runtime_error(errorMessage); });
}

}  // namespace

TEST(DockerClientUnitTest, ParsesContainerList) {
    const auto mockResponse = nlohmann::json::array({
        {
            {"Id", "abc123"},
            {"Names", nlohmann::json::array({"/mock-container"})},
            {"State", "running"},
        },
    });
    auto adapter = makeAdapterForListContainers(mockResponse);

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 1U);
    EXPECT_EQ(containers[0].id, "abc123");
    EXPECT_EQ(containers[0].name, "mock-container");
    EXPECT_EQ(containers[0].state, "running");
}

TEST(DockerClientUnitTest, ParsesEmptyContainerList) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    const auto containers = adapter.listContainers();
    EXPECT_TRUE(containers.empty());
}

TEST(DockerClientUnitTest, ThrowsOnErrorInResponse) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::object({{"error", "not an array"}}));

    EXPECT_THROW(adapter.listContainers(), std::runtime_error);
}

TEST(DockerClientUnitTest, HandlesContainersWithMissingFields) {
    const auto mockResponse = nlohmann::json::array({
        {{"Id", "abc123"}},
        {{"Names", nlohmann::json::array({"/only-name"})}},
    });
    auto adapter = makeAdapterForListContainers(mockResponse);

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 2U);
    EXPECT_EQ(containers[0].id, "abc123");
    EXPECT_TRUE(containers[0].name.empty());
    EXPECT_TRUE(containers[0].state.empty());
    EXPECT_TRUE(containers[1].id.empty());
    EXPECT_EQ(containers[1].name, "only-name");
}

TEST(DockerClientUnitTest, ParsesMultipleContainers) {
    const auto mockResponse = nlohmann::json::array({
        {{"Id", "aaa"}, {"Names", nlohmann::json::array({"/alpha"})}, {"State", "running"}},
        {{"Id", "bbb"}, {"Names", nlohmann::json::array({"/beta"})}, {"State", "exited"}},
        {{"Id", "ccc"}, {"Names", nlohmann::json::array({"/gamma"})}, {"State", "paused"}},
    });
    auto adapter = makeAdapterForListContainers(mockResponse);

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 3U);
    EXPECT_EQ(containers[0].name, "alpha");
    EXPECT_EQ(containers[1].name, "beta");
    EXPECT_EQ(containers[1].state, "exited");
    EXPECT_EQ(containers[2].name, "gamma");
    EXPECT_EQ(containers[2].state, "paused");
}

TEST(DockerClientUnitTest, PropagatesTransportErrors) {
    auto adapter = makeAdapterWithError("Connection refused");

    EXPECT_THROW(adapter.listContainers(), std::runtime_error);
}

TEST(DockerClientUnitTest, StopContainerCallsCorrectEndpoint) {
    bool wasCalled = false;
    const std::string containerId = "test-container-456";

    auto adapter = DockerClient([&wasCalled, &containerId](HttpMethod method, std::string_view endpoint) {
        wasCalled = true;
        EXPECT_EQ(method, HttpMethod::POST);
        EXPECT_EQ(endpoint, fmt::format("/containers/{}/stop?t=5", containerId));
        return HttpResponse{204, ""};
    });

    adapter.stopContainer(containerId);
    EXPECT_TRUE(wasCalled);
}

TEST(DockerClientUnitTest, StopContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon unreachable");

    EXPECT_THROW(adapter.stopContainer("any-id"), std::runtime_error);
}

TEST(DockerClientUnitTest, StopContainerWithEmptyIdThrows) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW(adapter.stopContainer(""), std::runtime_error);
}

TEST(DockerClientUnitTest, KillContainerCallsCorrectEndpoint) {
    bool wasCalled = false;
    const std::string containerId = "test-container-123";

    auto adapter = DockerClient([&wasCalled, &containerId](HttpMethod method, std::string_view endpoint) {
        wasCalled = true;
        EXPECT_EQ(method, HttpMethod::POST);
        EXPECT_EQ(endpoint, fmt::format("/containers/{}/kill", containerId));
        return HttpResponse{204, ""};
    });

    adapter.killContainer(containerId);
    EXPECT_TRUE(wasCalled);
}

TEST(DockerClientUnitTest, KillContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon is on fire");

    EXPECT_THROW(adapter.killContainer("any-id"), std::runtime_error);
}

TEST(DockerClientUnitTest, KillContainerWithEmptyIdThrows) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW(adapter.killContainer(""), std::runtime_error);
}
