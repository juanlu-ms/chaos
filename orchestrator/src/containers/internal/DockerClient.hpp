#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::containers::internal {

/**
 * @brief Supported HTTP methods for Docker Engine API calls.
 */
enum class HttpMethod : std::int8_t { GET, POST };

/**
 * @brief Raw HTTP response from Docker Engine API.
 */
struct HttpResponse {
    int status = 0;
    std::string body;
};

/**
 * @brief Docker Engine adapter implementing the container engine port.
 */
class DockerClient final : public chaos::orchestrator::containers::IContainerEngine {
public:
    /**
     * @brief Function used to execute API requests.
     * @param method HTTP method (GET or POST).
     * @param endpoint API endpoint (e.g., /containers/json).
     * @param body Optional request payload encoded as JSON.
     * @return HTTP response with status code and body.
     * @throws std::exception On transport errors.
     */
    using RequestFn = std::function<HttpResponse(HttpMethod, std::string_view, std::string_view)>;

    /**
     * @brief Construct an adapter using an injected request function.
     * @param requestFn Function that performs requests against Docker API.
     */
    explicit DockerClient(RequestFn requestFn);
    ~DockerClient() override = default;

    DockerClient(const DockerClient&) = delete;
    DockerClient& operator=(const DockerClient&) = delete;
    DockerClient(DockerClient&&) noexcept = default;
    DockerClient& operator=(DockerClient&&) noexcept = default;

    /**
     * @brief Create a default adapter using a Unix socket.
     * @param socketPath Path to Docker Engine Unix socket.
     * @return A container engine instance.
     */
    static std::shared_ptr<chaos::orchestrator::containers::IContainerEngine> create(
        const std::string& socketPath = "/var/run/docker.sock");

    /**
     * @brief List containers from the Docker Engine.
     * @return Vector of container summaries.
     * @throws std::exception On unexpected response formats.
     */
    std::vector<chaos::orchestrator::containers::Container> listContainers() override;

    /**
     * @brief Create a new container with the specified image and options.
     * @param image Container image to use (e.g. "nginx:latest").
     * @param options Additional options for container creation (e.g. env vars).
     * @throws std::exception On transport errors or non-OK responses.
     */
    void createContainer(const std::string_view image, const std::vector<std::string>& options) override;

    /**
     * @brief Start a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    void startContainer(const std::string_view containerId) override;

    /**
     * @brief Stop a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    void stopContainer(const std::string_view containerId) override;

    /**
     * @brief Kill a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    void killContainer(const std::string_view containerId) override;

    /**
     * @brief Execute a command inside a running container.
     * @param containerId ID of the target container.
     * @param command Command to execute (e.g. "ls -la /").
     * @return Output of the command execution.
     * @throws std::exception On transport errors or non-OK responses.
     */
    std::string exec(const std::string_view containerId, const std::string_view command) override;

private:
    /** @brief Function used to execute API requests. */
    RequestFn request_;

    /**
     * @brief Parse HTTP response body as JSON and handle errors.
     * @param response HTTP response to parse.
     * @return Parsed JSON object.
     * @throws std::exception On parsing failures.
     */
    nlohmann::json parseResponse(const HttpResponse& response) const;
};

}  // namespace chaos::orchestrator::containers::internal
