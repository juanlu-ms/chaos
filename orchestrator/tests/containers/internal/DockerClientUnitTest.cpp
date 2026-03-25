#include <fmt/format.h>
#include <gtest/gtest.h>

#include <containers/internal/DockerClient.hpp>
#include <nlohmann/json.hpp>
#include <string_view>

/**
 * @file DockerClientUnitTest.cpp
 * @brief Unit tests for DockerClient behavior using a mocked transport adapter.
 */

using chaos::orchestrator::containers::internal::DockerClient;
using chaos::orchestrator::containers::internal::HttpMethod;
using chaos::orchestrator::containers::internal::HttpResponse;

namespace {

/**
 * @brief Creates a DockerClient adapter that returns container list responses.
 */
DockerClient makeAdapterForListContainers(const nlohmann::json& response) {
    return DockerClient([response](HttpMethod method, std::string_view endpoint, std::string_view body) {
        EXPECT_EQ(method, HttpMethod::GET);
        EXPECT_EQ(endpoint, "/containers/json");
        EXPECT_TRUE(body.empty());
        return HttpResponse{.status = 200, .body = response.dump()};
    });
}

/**
 * @brief Creates a DockerClient adapter that throws a transport-level error.
 */
DockerClient makeAdapterWithError(const std::string& errorMessage) {
    return DockerClient([errorMessage](HttpMethod, std::string_view, std::string_view) -> HttpResponse {
        throw std::runtime_error(errorMessage);
    });
}

}  // namespace

/**
 * @test Verifies correct parsing of a single-container list.
 */
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

/**
 * @test Verifies an empty response produces an empty list.
 */
TEST(DockerClientUnitTest, ParsesEmptyContainerList) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    const auto containers = adapter.listContainers();
    EXPECT_TRUE(containers.empty());
}

/**
 * @test Verifies an invalid daemon response raises an exception.
 */
TEST(DockerClientUnitTest, ThrowsOnErrorInResponse) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::object({{"error", "not an array"}}));

    EXPECT_THROW(adapter.listContainers(), std::runtime_error);
}

/**
 * @test Verifies tolerant handling of containers with missing fields.
 */
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

/**
 * @test Verifies parsing of multiple containers with different states.
 */
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

/**
 * @test Verifies propagation of transport errors.
 */
TEST(DockerClientUnitTest, PropagatesTransportErrors) {
    auto adapter = makeAdapterWithError("Connection refused");

    EXPECT_THROW(adapter.listContainers(), std::runtime_error);
}

/**
 * @test Verifies stopContainer calls the expected endpoint and HTTP method.
 */
TEST(DockerClientUnitTest, StopContainerCallsCorrectEndpoint) {
    bool wasCalled = false;
    const std::string containerId = "test-container-456";

    auto adapter =
        DockerClient([&wasCalled, &containerId](HttpMethod method, std::string_view endpoint, std::string_view body) {
            wasCalled = true;
            EXPECT_EQ(method, HttpMethod::POST);
            EXPECT_EQ(endpoint, fmt::format("/containers/{}/stop?t=5", containerId));
            EXPECT_TRUE(body.empty());
            return HttpResponse{204, ""};
        });

    adapter.stopContainer(containerId);
    EXPECT_TRUE(wasCalled);
}

/**
 * @test Verifies stopContainer propagates API errors.
 */
TEST(DockerClientUnitTest, StopContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon unreachable");

    EXPECT_THROW(adapter.stopContainer("any-id"), std::runtime_error);
}

/**
 * @test Verifies stopContainer rejects empty container IDs.
 */
TEST(DockerClientUnitTest, StopContainerWithEmptyIdThrows) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW(adapter.stopContainer(""), std::invalid_argument);
}

/**
 * @test Verifies killContainer calls the expected endpoint and HTTP method.
 */
TEST(DockerClientUnitTest, KillContainerCallsCorrectEndpoint) {
    bool wasCalled = false;
    const std::string containerId = "test-container-123";

    auto adapter =
        DockerClient([&wasCalled, &containerId](HttpMethod method, std::string_view endpoint, std::string_view body) {
            wasCalled = true;
            EXPECT_EQ(method, HttpMethod::POST);
            EXPECT_EQ(endpoint, fmt::format("/containers/{}/kill", containerId));
            EXPECT_TRUE(body.empty());
            return HttpResponse{204, ""};
        });

    adapter.killContainer(containerId);
    EXPECT_TRUE(wasCalled);
}

/**
 * @test Verifies killContainer propagates API errors.
 */
TEST(DockerClientUnitTest, KillContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon is on fire");

    EXPECT_THROW(adapter.killContainer("any-id"), std::runtime_error);
}

/**
 * @test Verifies killContainer rejects empty container IDs.
 */
TEST(DockerClientUnitTest, KillContainerWithEmptyIdThrows) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW(adapter.killContainer(""), std::invalid_argument);
}

/**
 * @test Verifies exec performs create+start flow and returns command output.
 */
TEST(DockerClientUnitTest, ExecCallsCreateAndStartEndpoints) {
    const std::string containerId = "container-1";
    const std::string command = "echo hello";
    int calls = 0;

    auto adapter = DockerClient([&](HttpMethod method, std::string_view endpoint, std::string_view body) {
        EXPECT_EQ(method, HttpMethod::POST);
        ++calls;

        if (calls == 1) {
            EXPECT_EQ(endpoint, fmt::format("/containers/{}/exec", containerId));
            const auto payload = nlohmann::json::parse(body);
            EXPECT_TRUE(payload["AttachStdout"].get<bool>());
            EXPECT_TRUE(payload["AttachStderr"].get<bool>());
            EXPECT_FALSE(payload["Tty"].get<bool>());
            EXPECT_EQ(payload["Cmd"].at(0).get<std::string>(), "/bin/sh");
            EXPECT_EQ(payload["Cmd"].at(1).get<std::string>(), "-lc");
            EXPECT_EQ(payload["Cmd"].at(2).get<std::string>(), command);
            return HttpResponse{201, R"json({"Id":"exec-123"})json"};
        }

        EXPECT_EQ(endpoint, "/exec/exec-123/start");
        const auto payload = nlohmann::json::parse(body);
        EXPECT_FALSE(payload["Detach"].get<bool>());
        EXPECT_FALSE(payload["Tty"].get<bool>());
        return HttpResponse{200, "hello\n"};
    });

    const auto output = adapter.exec(containerId, command);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(output, "hello\n");
}

/**
 * @test Verifies exec validates empty input arguments.
 */
TEST(DockerClientUnitTest, ExecRejectsEmptyArguments) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW(adapter.exec("", "echo hi"), std::invalid_argument);
    EXPECT_THROW(adapter.exec("container", ""), std::invalid_argument);
}

/**
 * @test Verifies createContainer sends POST to /containers/create with image in JSON body.
 */
TEST(DockerClientUnitTest, CreateContainerSendsCorrectRequest) {
    bool wasCalled = false;
    std::string capturedBody;

    auto adapter = DockerClient([&wasCalled, &capturedBody](HttpMethod method, std::string_view endpoint,
                                                             std::string_view body) {
        wasCalled = true;
        EXPECT_EQ(method, HttpMethod::POST);
        EXPECT_EQ(endpoint, "/containers/create");
        capturedBody = std::string(body);
        return HttpResponse{201, R"json({"Id":"newly-created-id","Warnings":[]})json"};
    });

    adapter.createContainer("alpine:latest", {});
    EXPECT_TRUE(wasCalled);

    const auto payload = nlohmann::json::parse(capturedBody);
    EXPECT_EQ(payload.at("Image").get<std::string>(), "alpine:latest");
}

/**
 * @test Verifies createContainer includes Env options when provided.
 */
TEST(DockerClientUnitTest, CreateContainerIncludesEnvOptions) {
    std::string capturedBody;

    auto adapter =
        DockerClient([&capturedBody](HttpMethod, std::string_view, std::string_view body) {
            capturedBody = std::string(body);
            return HttpResponse{201, R"json({"Id":"test-id","Warnings":[]})json"};
        });

    adapter.createContainer("nginx:latest", {"FOO=bar", "BAZ=qux"});

    const auto payload = nlohmann::json::parse(capturedBody);
    ASSERT_TRUE(payload.at("Env").is_array());
    EXPECT_EQ(payload.at("Env").size(), 2U);
    EXPECT_EQ(payload.at("Env").at(0).get<std::string>(), "FOO=bar");
}

/**
 * @test Verifies createContainer throws when given an empty image name.
 */
TEST(DockerClientUnitTest, CreateContainerRejectsEmptyImage) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());
    EXPECT_THROW(adapter.createContainer("", {}), std::invalid_argument);
}

/**
 * @test Verifies createContainer propagates API errors.
 */
TEST(DockerClientUnitTest, CreateContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon unavailable");
    EXPECT_THROW(adapter.createContainer("alpine:latest", {}), std::runtime_error);
}

/**
 * @test Verifies getLogs calls the correct Docker API endpoint and returns body.
 */
TEST(DockerClientUnitTest, GetLogsCallsCorrectEndpoint) {
    const std::string expectedLogs = "app started\nsome error\n";
    DockerClient adapter([&expectedLogs](HttpMethod method, std::string_view endpoint, std::string_view body) {
        EXPECT_EQ(method, HttpMethod::GET);
        EXPECT_EQ(endpoint, "/containers/my-id/logs?stdout=1&stderr=1&timestamps=0");
        EXPECT_TRUE(body.empty());
        return HttpResponse{.status = 200, .body = expectedLogs};
    });
    EXPECT_EQ(adapter.getLogs("my-id"), expectedLogs);
}

/**
 * @test Verifies getLogs throws std::invalid_argument on empty container ID.
 */
TEST(DockerClientUnitTest, GetLogsWithEmptyIdThrows) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());
    EXPECT_THROW(adapter.getLogs(""), std::invalid_argument);
}

/**
 * @test Verifies getLogs throws ContainerEngineApiError on non-200 response.
 */
TEST(DockerClientUnitTest, GetLogsPropagatesApiError) {
    DockerClient adapter([](HttpMethod, std::string_view, std::string_view) {
        return HttpResponse{.status = 404, .body = "not found"};
    });
    EXPECT_THROW(adapter.getLogs("missing-container"),
                 chaos::orchestrator::containers::ContainerEngineApiError);
}

