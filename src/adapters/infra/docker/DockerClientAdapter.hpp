#pragma once

#include <domain/ports/IContainerEngine.hpp>
#include <functional>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "domain/entities/Container.hpp"

namespace chaos::adapters::infra::docker {

/**
 * @brief Supported HTTP methods for Docker Engine API calls.
 */
enum class HttpMethod { GET, POST };

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
class DockerClientAdapter : public chaos::domain::ports::IContainerEngine {
public:
    /**
     * @brief Function used to execute API requests.
     * @param method HTTP method (GET or POST).
     * @param endpoint API endpoint (e.g., /containers/json).
     * @return HTTP response with status code and body.
     * @throws std::exception On transport errors.
     */
    using RequestFn = std::function<HttpResponse(HttpMethod, const std::string&)>;

    /**
     * @brief Construct an adapter using an injected request function.
     * @param requestFn Function that performs requests against Docker API.
     */
    explicit DockerClientAdapter(RequestFn requestFn);
    ~DockerClientAdapter() override = default;

    DockerClientAdapter(const DockerClientAdapter&) = delete;
    DockerClientAdapter& operator=(const DockerClientAdapter&) = delete;
    DockerClientAdapter(DockerClientAdapter&&) noexcept = default;
    DockerClientAdapter& operator=(DockerClientAdapter&&) noexcept = default;

    /**
     * @brief Create a default adapter using a Unix socket.
     * @param socket_path Path to Docker Engine Unix socket.
     * @return A container engine instance.
     */
    static std::unique_ptr<chaos::domain::ports::IContainerEngine> create(
        const std::string& socket_path = "/var/run/docker.sock");

    /**
     * @brief List containers from the Docker Engine.
     * @return Vector of container summaries.
     * @throws std::exception On unexpected response formats.
     */
    std::vector<chaos::domain::Container> listContainers() override;
    /**
     * @brief Kill a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    void killContainer(const std::string& containerId) override;

private:
    RequestFn request_;

    nlohmann::json validateResponse(const HttpResponse& response);
};

}  // namespace chaos::adapters::infra::docker
