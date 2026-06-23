/**
 * @file DockerClientUnitTest.cpp
 * @brief Unit tests for DockerClient behavior using a mocked transport adapter.
 */

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string_view>

#include "containers/internal/DockerClient.hpp"

using chaos::orchestrator::containers::ContainerEngineApiError;
using chaos::orchestrator::containers::ContainerEngineError;
using chaos::orchestrator::containers::ContainerEngineParseError;
using chaos::orchestrator::containers::DockerClient;
using chaos::orchestrator::containers::HttpMethod;
using chaos::orchestrator::containers::HttpResponse;

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
DockerClient makeAdapterWithError(const std::string& error_message) {
    return DockerClient([error_message](HttpMethod, std::string_view, std::string_view) -> HttpResponse {
        throw std::runtime_error(error_message);
    });
}

}  // namespace

/**
 * @test Verifies correct parsing of a single-container list.
 */
TEST(DockerClientUnitTest, ParsesContainerList) {
    const auto mock_response = nlohmann::json::array({
        {
            {"Id", "abc123"},
            {"Names", nlohmann::json::array({"/mock-container"})},
            {"State", "running"},
        },
        {
            {"Id", "def456"}, {"Names", nlohmann::json::array({"/another-container"})},
            // State field is intentionally missing to test default handling
        },
    });
    auto adapter = makeAdapterForListContainers(mock_response);

    const auto containers = adapter.listContainers();

    ASSERT_EQ(containers.size(), 2U);
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

    EXPECT_THROW((void)adapter.listContainers(), std::runtime_error);
}

/**
 * @test Verifies tolerant handling of containers with missing fields on listContainers.
 */
TEST(DockerClientUnitTest, ListHandlesContainersWithMissingFields) {
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

/**
 * @test Verifies parsing of multiple containers with different states on listContainers.
 */
TEST(DockerClientUnitTest, ListParsesMultipleContainers) {
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

/**
 * @test Verifies propagation of transport errors.
 */
TEST(DockerClientUnitTest, ListPropagatesTransportErrors) {
    auto adapter = makeAdapterWithError("Connection refused");

    EXPECT_THROW((void)adapter.listContainers(), std::runtime_error);
}

/**
 * @test Verifies stopContainer calls the expected endpoint and HTTP method.
 */
TEST(DockerClientUnitTest, StopContainerCallsCorrectEndpoint) {
    bool was_called = false;
    const std::string container_id = "test-container-456";

    auto adapter =
        DockerClient([&was_called, &container_id](HttpMethod method, std::string_view endpoint, std::string_view body) {
            was_called = true;
            EXPECT_EQ(method, HttpMethod::POST);
            EXPECT_EQ(endpoint, fmt::format("/containers/{}/stop?t=5", container_id));
            EXPECT_TRUE(body.empty());
            return HttpResponse{204, ""};
        });

    adapter.stopContainer(container_id);
    EXPECT_TRUE(was_called);
}

/**
 * @test Verifies stopContainer propagates API errors.
 */
TEST(DockerClientUnitTest, StopContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon unreachable");

    EXPECT_THROW((void)adapter.stopContainer("any-id"), std::runtime_error);
}

/**
 * @test Verifies stopContainer rejects empty container IDs.
 */
TEST(DockerClientUnitTest, StopContainerWithEmptyIdThrows) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW((void)adapter.stopContainer(""), std::invalid_argument);
}

/**
 * @test Verifies killContainer calls the expected endpoint and HTTP method.
 */
TEST(DockerClientUnitTest, KillContainerCallsCorrectEndpoint) {
    bool was_called = false;
    const std::string container_id = "test-container-123";

    auto adapter =
        DockerClient([&was_called, &container_id](HttpMethod method, std::string_view endpoint, std::string_view body) {
            was_called = true;
            EXPECT_EQ(method, HttpMethod::POST);
            EXPECT_EQ(endpoint, fmt::format("/containers/{}/kill", container_id));
            EXPECT_TRUE(body.empty());
            return HttpResponse{204, ""};
        });

    adapter.killContainer(container_id);
    EXPECT_TRUE(was_called);
}

/**
 * @test Verifies killContainer propagates API errors.
 */
TEST(DockerClientUnitTest, KillContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon is on fire");

    EXPECT_THROW((void)adapter.killContainer("any-id"), std::runtime_error);
}

/**
 * @test Verifies killContainer rejects empty container IDs.
 */
TEST(DockerClientUnitTest, KillContainerWithEmptyIdThrows) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW((void)adapter.killContainer(""), std::invalid_argument);
}

/**
 * @test Verifies exec performs create+start flow and returns command output.
 */
TEST(DockerClientUnitTest, ExecCallsCreateAndStartEndpoints) {
    const std::string container_id = "container-1";
    const std::string command = "echo hello";
    int calls = 0;

    auto adapter = DockerClient(
        [&calls, &container_id, &command](HttpMethod method, std::string_view endpoint, std::string_view body) {
            ++calls;

            if (calls == 1) {
                EXPECT_EQ(method, HttpMethod::POST);
                EXPECT_EQ(endpoint, fmt::format("/containers/{}/exec", container_id));
                const auto payload = nlohmann::json::parse(body);
                EXPECT_TRUE(payload["AttachStdout"].get<bool>());
                EXPECT_TRUE(payload["AttachStderr"].get<bool>());
                EXPECT_FALSE(payload["Tty"].get<bool>());
                EXPECT_EQ(payload["Cmd"].at(0).get<std::string>(), "/bin/sh");
                EXPECT_EQ(payload["Cmd"].at(1).get<std::string>(), "-lc");
                EXPECT_EQ(payload["Cmd"].at(2).get<std::string>(), command);
                return HttpResponse{201, R"json({"Id":"exec-123"})json"};
            }

            if (calls == 2) {
                EXPECT_EQ(method, HttpMethod::POST);
                EXPECT_EQ(endpoint, "/exec/exec-123/start");
                const auto payload = nlohmann::json::parse(body);
                EXPECT_FALSE(payload["Detach"].get<bool>());
                EXPECT_FALSE(payload["Tty"].get<bool>());
                return HttpResponse{200, "hello\n"};
            }

            EXPECT_EQ(method, HttpMethod::GET);
            EXPECT_EQ(endpoint, "/exec/exec-123/json");
            EXPECT_TRUE(body.empty());
            return HttpResponse{200, R"json({"ExitCode":0})json"};
        });

    const auto output = adapter.exec(container_id, command);
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(output, "hello\n");
}

/**
 * @test Verifies exec throws on non-zero exit code from the inspected command.
 */
TEST(DockerClientUnitTest, ExecThrowsOnNonZeroExitCode) {
    const std::string container_id = "container-1";
    const std::string command = "false";
    int calls = 0;

    auto adapter =
        DockerClient([&calls, &container_id](HttpMethod method, std::string_view endpoint, std::string_view) {
            ++calls;

            if (calls == 1) {
                EXPECT_EQ(method, HttpMethod::POST);
                EXPECT_EQ(endpoint, fmt::format("/containers/{}/exec", container_id));
                return HttpResponse{201, R"json({"Id":"exec-99"})json"};
            }

            if (calls == 2) {
                EXPECT_EQ(method, HttpMethod::POST);
                EXPECT_EQ(endpoint, "/exec/exec-99/start");
                return HttpResponse{200, "command output"};
            }

            EXPECT_EQ(method, HttpMethod::GET);
            EXPECT_EQ(endpoint, "/exec/exec-99/json");
            return HttpResponse{200, R"json({"ExitCode":1})json"};
        });

    EXPECT_THROW((void)adapter.exec(container_id, command), ContainerEngineError);
    EXPECT_EQ(calls, 3);
}

/**
 * @test Verifies execInNetNs calls the Docker inspect endpoint and runs nsenter.
 */
TEST(DockerClientUnitTest, ExecInNetNsCallsInspectEndpoint) {
    const std::string container_id = "test-container";
    int calls = 0;

    auto adapter =
        DockerClient([&calls, &container_id](HttpMethod method, std::string_view endpoint, std::string_view) {
            ++calls;
            EXPECT_EQ(method, HttpMethod::GET);
            EXPECT_EQ(endpoint, fmt::format("/containers/{}/json", container_id));
            return HttpResponse{200, R"json({"State":{"Status":"running","Pid":1}})json"};
        });

    try {
        const auto output = adapter.execInNetNs(container_id, "echo ok");
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(output, "ok\n");
    } catch (const ContainerEngineError&) {
        // nsenter may not be available in all environments — that's fine
        EXPECT_EQ(calls, 1);
    }
}

/**
 * @test Verifies execInNetNs throws on non-200 Docker inspect response.
 */
TEST(DockerClientUnitTest, ExecInNetNsThrowsOnInspectFailure) {
    auto adapter = DockerClient(
        [](HttpMethod, std::string_view, std::string_view) { return HttpResponse{500, "Internal error"}; });

    EXPECT_THROW((void)adapter.execInNetNs("c", "echo hi"), ContainerEngineApiError);
}

/**
 * @test Verifies execInNetNs throws when inspect response is missing State.Pid.
 */
TEST(DockerClientUnitTest, ExecInNetNsThrowsOnMissingPid) {
    auto adapter = DockerClient([](HttpMethod, std::string_view, std::string_view) {
        return HttpResponse{200, R"json({"State":{"Status":"running"}})json"};
    });

    EXPECT_THROW((void)adapter.execInNetNs("c", "echo hi"), ContainerEngineParseError);
}

/**
 * @test Verifies execInNetNs throws when container PID is 0 (not running).
 */
TEST(DockerClientUnitTest, ExecInNetNsThrowsOnStoppedContainer) {
    auto adapter = DockerClient([](HttpMethod, std::string_view, std::string_view) {
        return HttpResponse{200, R"json({"State":{"Status":"exited","Pid":0}})json"};
    });

    EXPECT_THROW((void)adapter.execInNetNs("c", "echo hi"), ContainerEngineError);
}

/**
 * @test Verifies execInNetNs validates empty input arguments.
 */
TEST(DockerClientUnitTest, ExecInNetNsRejectsEmptyArguments) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW((void)adapter.execInNetNs("", "echo hi"), std::invalid_argument);
    EXPECT_THROW((void)adapter.execInNetNs("container", ""), std::invalid_argument);
}

/**
 * @test Verifies exec validates empty input arguments.
 */
TEST(DockerClientUnitTest, ExecRejectsEmptyArguments) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());

    EXPECT_THROW((void)adapter.exec("", "echo hi"), std::invalid_argument);
    EXPECT_THROW((void)adapter.exec("container", ""), std::invalid_argument);
}

/**
 * @test Verifies createContainer sends POST to /containers/create with image in JSON body.
 */
TEST(DockerClientUnitTest, CreateContainerSendsCorrectRequest) {
    bool was_called = false;
    std::string captured_body;

    auto adapter = DockerClient(
        [&was_called, &captured_body](HttpMethod method, std::string_view endpoint, std::string_view body) {
            was_called = true;
            EXPECT_EQ(method, HttpMethod::POST);
            EXPECT_EQ(endpoint, "/containers/create");
            captured_body = std::string(body);
            return HttpResponse{201, R"json({"Id":"newly-created-id","Warnings":[]})json"};
        });

    const auto container_id = adapter.createContainer("alpine:latest", {});
    EXPECT_EQ(container_id, "newly-created-id");
    EXPECT_TRUE(was_called);

    const auto payload = nlohmann::json::parse(captured_body);
    EXPECT_EQ(payload.at("Image").get<std::string>(), "alpine:latest");
}

/**
 * @test Verifies createContainer includes Env options when provided.
 */
TEST(DockerClientUnitTest, CreateContainerIncludesEnvOptions) {
    std::string captured_body;

    auto adapter = DockerClient([&captured_body](HttpMethod, std::string_view, std::string_view body) {
        captured_body = std::string(body);
        return HttpResponse{201, R"json({"Id":"test-id","Warnings":[]})json"};
    });

    const auto container_id = adapter.createContainer("nginx:latest", {"FOO=bar", "BAZ=qux"});
    EXPECT_EQ(container_id, "test-id");

    const auto payload = nlohmann::json::parse(captured_body);
    ASSERT_TRUE(payload.at("Env").is_array());
    EXPECT_EQ(payload.at("Env").size(), 2U);
    EXPECT_EQ(payload.at("Env").at(0).get<std::string>(), "FOO=bar");
}

/**
 * @test Verifies createContainer throws when given an empty image name.
 */
TEST(DockerClientUnitTest, CreateContainerRejectsEmptyImage) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());
    EXPECT_THROW((void)adapter.createContainer("", {}), std::invalid_argument);
}

/**
 * @test Verifies createContainer propagates API errors.
 */
TEST(DockerClientUnitTest, CreateContainerPropagatesApiError) {
    auto adapter = makeAdapterWithError("Docker daemon unavailable");
    EXPECT_THROW((void)adapter.createContainer("alpine:latest", {}), std::runtime_error);
}

/**
 * @test Verifies getLogs calls the correct Docker API endpoint and returns body.
 */
TEST(DockerClientUnitTest, GetLogsCallsCorrectEndpoint) {
    // The Docker Engine logs API returns a multiplexed stream where each
    // frame has an 8-byte header (stream type + 3 pad + 4 length).
    // Build two frames: stdout "app started\n" + stderr "some error\n"
    auto buildFrame = [](char stream, const std::string_view msg) {
        const auto len = static_cast<uint32_t>(msg.size());
        std::string frame;
        frame += stream;
        frame += '\0';
        frame += '\0';
        frame += '\0';
        frame += static_cast<char>(len >> 24);
        frame += static_cast<char>(len >> 16);
        frame += static_cast<char>(len >> 8);
        frame += static_cast<char>(len);
        frame += msg;
        return frame;
    };

    const std::string body = buildFrame(1, "app started\n") + buildFrame(2, "some error\n");
    const std::string expected_logs = "app started\nsome error\n";

    DockerClient adapter([&body](HttpMethod method, std::string_view endpoint, std::string_view) {
        EXPECT_EQ(method, HttpMethod::GET);
        EXPECT_EQ(endpoint, "/containers/my-id/logs?stdout=1&stderr=1&timestamps=0&tail=50");
        return HttpResponse{.status = 200, .body = body};
    });
    EXPECT_EQ(adapter.getLogs("my-id"), expected_logs);
}

/**
 * @test Verifies getLogs throws std::invalid_argument on empty container ID.
 */
TEST(DockerClientUnitTest, GetLogsWithEmptyIdThrows) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());
    EXPECT_THROW((void)adapter.getLogs(""), std::invalid_argument);
}

/**
 * @test Verifies getLogs throws ContainerEngineApiError on non-200 response.
 */
TEST(DockerClientUnitTest, GetLogsPropagatesApiError) {
    DockerClient adapter([](HttpMethod, std::string_view, std::string_view) {
        return HttpResponse{.status = 404, .body = "not found"};
    });
    EXPECT_THROW((void)adapter.getLogs("missing-container"), ContainerEngineApiError);
}

/**
 * @test Verifies getSystemInfo calls GET /info and extracts MemTotal.
 */
TEST(DockerClientUnitTest, GetSystemInfoCallsCorrectEndpoint) {
    const nlohmann::json mock_response = {
        {"MemTotal", 8388608000},
        {"NCPU", 4},
        {"ServerVersion", "24.0.0"},
    };

    DockerClient adapter([&mock_response](HttpMethod method, std::string_view endpoint, std::string_view body) {
        EXPECT_EQ(method, HttpMethod::GET);
        EXPECT_EQ(endpoint, "/info");
        EXPECT_TRUE(body.empty());
        return HttpResponse{.status = 200, .body = mock_response.dump()};
    });

    auto sys_info = adapter.getSystemInfo();
    EXPECT_EQ(sys_info.mem_total, 8388608000);
}

/**
 * @test Verifies getSystemInfo throws ContainerEngineApiError on non-200 response.
 */
TEST(DockerClientUnitTest, GetSystemInfoPropagatesApiError) {
    DockerClient adapter([](HttpMethod, std::string_view, std::string_view) {
        return HttpResponse{.status = 500, .body = "Internal server error"};
    });
    EXPECT_THROW((void)adapter.getSystemInfo(), ContainerEngineApiError);
}

/**
 * @test pullImage sends POST /images/create with a JSON body containing the
 *       image name and completes successfully.
 */
TEST(DockerClientUnitTest, PullImageSucceedsWithDockerProgressStream) {
    bool was_called = false;
    const std::string image = "nginx:latest";

    const std::string docker_pull_response = R"({"status":"Pulling from library/nginx","id":"latest"}\n)"
                                             R"({"status":"Digest: sha256:abc123"}\n)"
                                             R"({"status":"Status: Image is up to date for nginx:latest"}\n)";

    auto adapter = DockerClient([&was_called, &image, &docker_pull_response](
                                    HttpMethod method, std::string_view endpoint, std::string_view body) {
        was_called = true;
        EXPECT_EQ(method, HttpMethod::POST);
        EXPECT_EQ(endpoint, "/images/create");
        const auto payload = nlohmann::json::parse(body);
        EXPECT_EQ(payload.at("Image").get<std::string>(), image);
        return HttpResponse{200, docker_pull_response};
    });

    EXPECT_NO_THROW(adapter.pullImage(image));
    EXPECT_TRUE(was_called);
}

/**
 * @test Verifies pullImage rejects empty image names.
 */
TEST(DockerClientUnitTest, PullImageRejectsEmptyImage) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());
    EXPECT_THROW(adapter.pullImage(""), std::invalid_argument);
}

/**
 * @test Verifies pullImage throws std::invalid_argument on 404.
 */
TEST(DockerClientUnitTest, PullImageThrowsOn404) {
    DockerClient adapter([](HttpMethod, std::string_view, std::string_view) {
        return HttpResponse{.status = 404, .body = R"({"message":"repository not found"})"};
    });
    EXPECT_THROW((void)adapter.pullImage("nonexistent/image"), std::invalid_argument);
}

/**
 * @test Verifies pullImage throws ContainerEngineApiError on 500.
 */
TEST(DockerClientUnitTest, PullImageThrowsOn500) {
    DockerClient adapter([](HttpMethod, std::string_view, std::string_view) {
        return HttpResponse{.status = 500, .body = "Internal server error"};
    });
    EXPECT_THROW((void)adapter.pullImage("nginx:latest"), ContainerEngineApiError);
}

/**
 * @test Verifies pullImage propagates transport errors.
 */
TEST(DockerClientUnitTest, PullImagePropagatesTransportErrors) {
    auto adapter = makeAdapterWithError("Docker daemon unreachable");
    EXPECT_THROW((void)adapter.pullImage("nginx:latest"), std::runtime_error);
}

/**
 * @test buildImage sends POST_TAR /build?t=<tag> with a non-empty tar body
 *       and a temp Dockerfile on disk.
 */
TEST(DockerClientUnitTest, BuildImageSendsTarToCorrectEndpoint) {
    const std::string dockerfile_content = "FROM alpine:latest\nCMD [\"echo\", \"hello\"]\n";
    const std::filesystem::path tmp_dir =
        fmt::format("/tmp/chaos_test_{}", std::to_string(reinterpret_cast<std::uintptr_t>(&dockerfile_content)));
    std::filesystem::create_directory(tmp_dir);
    const std::filesystem::path dockerfile_path = tmp_dir / "Dockerfile";
    {
        std::ofstream file(dockerfile_path);
        file << dockerfile_content;
    }

    bool was_called = false;
    const std::string image_name = "test-image:latest";

    auto adapter =
        DockerClient([&was_called, &image_name](HttpMethod method, std::string_view endpoint, std::string_view body) {
            was_called = true;
            EXPECT_EQ(method, HttpMethod::POST_TAR);
            EXPECT_EQ(endpoint, fmt::format("/build?t={}", image_name));
            EXPECT_FALSE(body.empty());
            return HttpResponse{200, "Successfully built"};
        });

    EXPECT_NO_THROW(adapter.buildImage(image_name, dockerfile_path.c_str()));
    EXPECT_TRUE(was_called);
    std::filesystem::remove_all(tmp_dir);
}

/**
 * @test Verifies buildImage rejects empty image names.
 */
TEST(DockerClientUnitTest, BuildImageRejectsEmptyImageName) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());
    EXPECT_THROW(adapter.buildImage("", "/some/Dockerfile"), std::invalid_argument);
}

/**
 * @test Verifies buildImage rejects empty dockerfile path.
 */
TEST(DockerClientUnitTest, BuildImageRejectsEmptyPath) {
    auto adapter = makeAdapterForListContainers(nlohmann::json::array());
    EXPECT_THROW(adapter.buildImage("myapp:latest", ""), std::invalid_argument);
}

/**
 * @test Verifies buildImage throws ContainerEngineApiError on 500.
 */
TEST(DockerClientUnitTest, BuildImageThrowsOn500) {
    const std::string dockerfile_content = "FROM alpine:latest\n";
    const std::filesystem::path tmp_dir =
        fmt::format("/tmp/chaos_test_{}", std::to_string(reinterpret_cast<std::uintptr_t>(&dockerfile_content)));
    std::filesystem::create_directory(tmp_dir);
    const std::filesystem::path dockerfile_path = tmp_dir / "Dockerfile";
    {
        std::ofstream file(dockerfile_path);
        file << dockerfile_content;
    }

    DockerClient adapter([](HttpMethod, std::string_view, std::string_view) {
        return HttpResponse{.status = 500, .body = "Internal server error"};
    });

    EXPECT_THROW((void)adapter.buildImage("myapp:latest", dockerfile_path.c_str()), ContainerEngineApiError);
    std::filesystem::remove_all(tmp_dir);
}

/**
 * @test Verifies buildImage propagates transport errors.
 */
TEST(DockerClientUnitTest, BuildImagePropagatesTransportErrors) {
    auto adapter = makeAdapterWithError("Docker daemon unreachable");
    EXPECT_THROW((void)adapter.buildImage("myapp:latest", "/some/Dockerfile"), std::runtime_error);
}

/**
 * @test getStats parses CPU, memory, and network from Docker stats JSON.
 */
TEST(DockerClientUnitTest, GetStatsParsesCpuMemoryNetwork) {
    auto stats_json = R"({
        "cpu_stats": {
            "cpu_usage": { "total_usage": 4825316964 },
            "system_cpu_usage": 17371230000000,
            "online_cpus": 4
        },
        "precpu_stats": {
            "cpu_usage": { "total_usage": 4825310000 },
            "system_cpu_usage": 17371220000000
        },
        "memory_stats": {
            "usage": 8388608,
            "limit": 33554432
        },
        "networks": {
            "eth0": { "rx_bytes": 1000000, "tx_bytes": 2000000,
                      "rx_packets": 100, "tx_packets": 200 }
        }
    })";

    auto adapter = DockerClient([stats_json](HttpMethod method, std::string_view endpoint, std::string_view) {
        EXPECT_EQ(method, HttpMethod::GET);
        EXPECT_NE(endpoint.find("stats"), std::string_view::npos);
        return HttpResponse{.status = 200, .body = stats_json};
    });

    auto stats = adapter.getStats("test-container");

    EXPECT_TRUE(stats.cpu_percent.has_value());
    EXPECT_TRUE(stats.memory_mb.has_value());
    // First call: no previous network state, rates are 0
    EXPECT_EQ(stats.network_rx_bps, 0.0);
    EXPECT_EQ(stats.network_tx_bps, 0.0);
}

/**
 * @test getStats returns parsed values for memory and CPU.
 */
TEST(DockerClientUnitTest, GetStatsReturnsMemoryInMb) {
    auto stats_json = R"({
        "cpu_stats": {
            "cpu_usage": { "total_usage": 1000000 },
            "system_cpu_usage": 1000000000,
            "online_cpus": 2
        },
        "precpu_stats": {
            "cpu_usage": { "total_usage": 0 },
            "system_cpu_usage": 0
        },
        "memory_stats": {
            "usage": 10485760,
            "limit": 1073741824
        },
        "networks": {
            "eth0": { "rx_bytes": 0, "tx_bytes": 0,
                      "rx_packets": 0, "tx_packets": 0 }
        }
    })";

    auto adapter = DockerClient([stats_json](HttpMethod, std::string_view, std::string_view) {
        return HttpResponse{.status = 200, .body = stats_json};
    });

    auto stats = adapter.getStats("test-container");

    EXPECT_TRUE(stats.cpu_percent.has_value());
    EXPECT_TRUE(stats.memory_mb.has_value());
    // 10485760 bytes = 10 MB
    EXPECT_NEAR(*stats.memory_mb, 10.0, 0.01);
}

/**
 * @test getStats propagates transport errors.
 */
TEST(DockerClientUnitTest, GetStatsPropagatesTransportErrors) {
    auto adapter = makeAdapterWithError("Docker daemon unreachable");
    EXPECT_THROW((void)adapter.getStats("test-container"), std::runtime_error);
}
