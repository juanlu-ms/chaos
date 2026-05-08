/**
 * @file DockerClient.hpp
 * @brief Docker Engine adapter implementing the container engine port.
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "containers/IContainerEngine.hpp"
#include "shared/ContainerStatus.hpp"

namespace chaos::orchestrator::containers::internal {

/**
 * @brief Supported HTTP methods for Docker Engine API calls.
 */
enum class HttpMethod : std::int8_t { GET, POST, POST_TAR, REMOVE };

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
class DockerClient final : public containers::IContainerEngine {
public:
    /**
     * @brief Function used to execute API requests.
     * @param method HTTP method (GET, POST, POST_TAR, REMOVE).
     * @param endpoint API endpoint (e.g., /containers/json).
     * @param body Optional request payload (JSON or tar archive).
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
    static std::shared_ptr<containers::IContainerEngine> create(const std::string& socketPath = "/var/run/docker.sock");

    /**
     * @brief List containers from the Docker Engine.
     * @return Vector of container summaries.
     * @throws std::exception On unexpected response formats.
     */
    [[nodiscard]] std::vector<containers::Container> listContainers() const override;

    /**
     * @brief Pull a container image from Docker Hub.
     * @param image Container image to pull (e.g. "nginx:latest").
     * @throws std::exception On transport errors or non-OK responses.
     */
    void pullImage(const std::string_view image) const override;

    /**
     * @brief Build a container image from a Dockerfile.
     * @param imageName Tag for the built image (e.g. "myapp:latest").
     * @param dockerfilePath Path to the Dockerfile on disk.
     * @throws std::invalid_argument On empty image name or path.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws std::runtime_error On filesystem errors.
     */
    void buildImage(const std::string_view imageName, const std::string_view dockerfilePath) const override;

    /**
     * @brief Create a new container with the specified image and options.
     * @param image Container image to use (e.g. "nginx:latest").
     * @param options Additional options for container creation.
     * @return The ID of the newly created container.
     * @throws std::invalid_argument On empty image name.
     * @throws ContainerEngineApiError On non-201 HTTP response.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] std::string createContainer(const std::string_view image,
                                              const std::vector<std::string>& options) const override;

    /**
     * @brief Start a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    void startContainer(const std::string_view containerId) const override;

    /**
     * @brief Stop a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    void stopContainer(const std::string_view containerId) const override;

    /**
     * @brief Kill a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    void killContainer(const std::string_view containerId) const override;

    /**
     * @brief Remove a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    void removeContainer(const std::string_view containerId) const override;

    /**
     * @brief Execute a command inside a running container.
     * @param containerId ID of the target container.
     * @param command Command to execute (e.g. "ls -la /").
     * @return Output of the command execution.
     * @throws std::exception On transport errors or non-OK responses.
     */
    [[nodiscard]] std::string exec(const std::string_view containerId, const std::string_view command) const override;

    /**
     * @brief Update the memory limit of a container via Docker Engine API.
     * @param containerId Docker container ID.
     * @param memory_bytes Max memory in bytes (0 to ignore).
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     */
    void updateMemoryLimit(const std::string_view containerId, int64_t memory_bytes) const override;

    /**
     * @brief Update the CPU quota and period of a container via Docker Engine API.
     * @param containerId Docker container ID.
     * @param cpu_quota CPU quota in microseconds (0 to ignore, -1 to reset/unlimited).
     * @param cpu_period CPU period in microseconds (0 to ignore).
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     */
    void updateCpuQuota(const std::string_view containerId, int64_t cpu_quota, int64_t cpu_period) const override;

    /**
     * @brief Get status of a container by ID.
     * @param containerId Docker container ID.
     * @return ContainerStatus enum value representing the container's state.
     * @throws ContainerEngineApiError On non-OK HTTP responses.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] shared::ContainerStatus getStatus(const std::string_view containerId) const override;

    /**
     * @brief Fetch stdout/stderr logs from a container via Docker Engine API.
     * @param containerId Docker container ID.
     * @return Raw log text (may contain Docker multiplexed stream headers).
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineTransportError On connection failure.
     */
    [[nodiscard]] std::string getLogs(const std::string_view containerId) const override;

    /**
     * @brief Compute network I/O rate (B/s) from cumulative Docker stats.
     * @return Pair of (rx_bps, tx_bps).
     */
    [[nodiscard]] std::pair<double, double> getContainerNetworkBps(const std::string_view containerId) const override;

    /**
     * @brief Fetch the memory usage of a running container in MB.
     * @param containerId Docker container ID.
     * @return Memory usage in MB.
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On unexpected response format.
     */
    [[nodiscard]] double getContainerMemoryUsage(const std::string_view containerId) const override;

    /**
     * @brief Fetch the CPU core limit configured for a running container.
     * @param containerId Docker container ID.
     * @return Configured CPU core limit (supports decimals, e.g. 0.5, 1.25, 2.0).
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On unexpected response format.
     */
    [[nodiscard]] double getContainerCpuUsage(const std::string_view containerId) const override;

    /**
     * @brief Fetch the primary IP address of a running container.
     * @param containerId Docker container ID.
     * @return IPv4 address as a string.
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On unexpected response format.
     */
    [[nodiscard]] std::string getContainerIp(const std::string_view containerId) const override;

    /**
     * @brief Retrieve system-level information from the Docker Engine.
     * @return SystemInfo containing total memory and other host information.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] SystemInfo getSystemInfo() const override;

private:
    /** @brief Function used to execute API requests. */
    RequestFn request_;

    // ── Response caches ─────────────────────────────────────────────
    // Docker inspect and stats responses are re-parsed by two separate
    // public methods each (status+ip, memory+cpu).  Caching avoids
    // doubling the HTTP calls on every observation tick.

    static constexpr auto INSPECT_TTL = std::chrono::milliseconds(2000);
    static constexpr auto STATS_TTL  = std::chrono::milliseconds(2000);
    static constexpr auto LOGS_TTL   = std::chrono::milliseconds(1500);

    mutable std::string cached_inspect_;
    mutable std::chrono::steady_clock::time_point cached_inspect_at_;
    mutable bool cached_inspect_valid_ = false;

    mutable std::string cached_stats_;
    mutable std::chrono::steady_clock::time_point cached_stats_at_;
    mutable bool cached_stats_valid_ = false;

    mutable std::string cached_logs_;
    mutable std::chrono::steady_clock::time_point cached_logs_at_;
    mutable bool cached_logs_valid_ = false;

    // Network B/s tracking: store previous cumulative bytes and timestamp
    // so we can compute the delta on each call.
    mutable double prev_net_rx_ = 0;
    mutable double prev_net_tx_ = 0;
    mutable std::chrono::steady_clock::time_point prev_net_timestamp_;
    mutable bool prev_net_valid_ = false;

    /**
     * @brief GET helper with transparent TTL caching.
     * @return Live (fresh) response body.
     */
    [[nodiscard]] std::string getCached(const std::string& endpoint, std::string& cacheBody,
                                        std::chrono::steady_clock::time_point& cacheTime, bool& cacheValid,
                                        std::chrono::milliseconds ttl) const;

    /**
     * @brief Parse HTTP response body as JSON and handle errors.
     * @param response HTTP response to parse.
     * @return Parsed JSON object.
     * @throws ContainerEngineParseError If response body is empty or cannot be parsed as JSON.
     * @throws ContainerEngineApiError If response status is not 200.
     */
    nlohmann::json parseResponse(const HttpResponse& response) const;
};

}  // namespace chaos::orchestrator::containers::internal
