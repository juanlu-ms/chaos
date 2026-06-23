/**
 * @file IContainerEngine.hpp
 * @brief Port for container engine operations and related exceptions.
 */

#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Container.hpp"
#include "ContainerStatus.hpp"
#include "SystemInfo.hpp"

namespace chaos::orchestrator::containers {

/** @brief Base exception for container engine errors. */
class ContainerEngineError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/** @brief Exception thrown for transport-level errors. */
class ContainerEngineTransportError : public ContainerEngineError {
public:
    using ContainerEngineError::ContainerEngineError;
};

/** @brief Exception thrown for API-level errors. */
class ContainerEngineApiError : public ContainerEngineError {
public:
    using ContainerEngineError::ContainerEngineError;
};

/** @brief Exception thrown for response parsing errors. */
class ContainerEngineParseError : public ContainerEngineError {
public:
    using ContainerEngineError::ContainerEngineError;
};

/**
 * @brief Snapshot of container resource stats from a single API call.
 */
struct ContainerStats {
    /** @brief CPU usage as a percentage of allocated CPU, if available. */
    std::optional<double> cpu_percent;
    /** @brief Memory usage in megabytes, if available. */
    std::optional<double> memory_mb;
    /** @brief Network receive rate in bytes per second. */
    double network_rx_bps = 0;
    /** @brief Network transmit rate in bytes per second. */
    double network_tx_bps = 0;
};

/**
 * @brief Port for container engine operations.
 */
class IContainerEngine {
public:
    virtual ~IContainerEngine() = default;

    /**
     * @brief List containers available in the engine.
     * @return Vector of container summaries.
     * @throws ContainerEngineError On transport, API or parsing failures.
     */
    [[nodiscard]] virtual std::vector<Container> listContainers() const = 0;

    /**
     * @brief Pull a container image from the registry.
     * @param image Container image to pull (e.g. "nginx:latest").
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void pullImage(const std::string_view image) const = 0;

    /**
     * @brief Build a container image from a Dockerfile.
     * @param image_name Tag for the built image (e.g. "myapp:latest").
     * @param dockerfile_path Path to the Dockerfile on disk.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     * @throws std::runtime_error On filesystem errors.
     */
    virtual void buildImage(const std::string_view imageName, const std::string_view dockerfilePath) const = 0;

    /**
     * @brief Create a new container with the specified image and options.
     * @param image Container image to use (e.g. "nginx:latest").
     * @param options Additional options for container creation (e.g. env vars).
     * @return The ID of the newly created container.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    [[nodiscard]] virtual std::string createContainer(const std::string_view image,
                                                      const std::vector<std::string>& options) const = 0;

    /**
     * @brief Start a container by ID.
     * @param containerId Docker container ID.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void startContainer(const std::string_view containerId) const = 0;

    /**
     * @brief Stop a container by ID.
     * @param containerId Container ID to stop.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void stopContainer(const std::string_view containerId) const = 0;

    /**
     * @brief Kill a container by ID.
     * @param containerId Docker container ID.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void killContainer(const std::string_view containerId) const = 0;

    /**
     * @brief Remove a container by ID.
     * @param containerId Docker container ID.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void removeContainer(const std::string_view containerId) const = 0;

    /**
     * @brief Execute a command inside a running container.
     * @param containerId ID of the target container.
     * @param command Command to execute (e.g. "ls -la /").
     * @return Output of the command execution.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    [[nodiscard]] virtual std::string exec(const std::string_view containerId,
                                           const std::string_view command) const = 0;

    /**
     * @brief Execute a command in the target container's network namespace from the host.
     *
     * Runs @p command on the host via nsenter, entering the container's network
     * namespace so that tools like tc(8) and iptables(8) affect the container
     * without needing them installed inside it.
     *
     * @param containerId ID of the target container.
     * @param command Command to execute on the host inside the container's netns.
     * @return Combined stdout+stderr of the command.
     * @throws ContainerEngineError If the container PID cannot be retrieved, or
     *         if the nsenter'd command exits with a non-zero status.
     */
    [[nodiscard]] virtual std::string execInNetNs(const std::string_view containerId,
                                                  const std::string_view command) const = 0;

    /**
     * @brief Get status of a container by ID.
     * @param containerId Docker container ID.
     * @return ContainerStatus enum value representing the container's state.
     * @throws ContainerEngineApiError On non-OK HTTP responses.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] virtual containers::ContainerStatus getStatus(const std::string_view containerId) const = 0;

    /**
     * @brief Fetch stdout/stderr logs for a container.
     * @param containerId Docker container ID.
     * @return Raw log text (stdout/stderr combined).
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] virtual std::string getLogs(const std::string_view containerId) const = 0;

    /**
     * @brief Update the memory limit of a container.
     * @param containerId ID of the target container.
     * @param memory_bytes Max memory in bytes (0 to ignore/reset).
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     */
    virtual void updateMemoryLimit(const std::string_view containerId, int64_t memory_bytes) const = 0;

    /**
     * @brief Update the CPU quota and period of a container.
     * @param containerId ID of the target container.
     * @param cpu_quota CPU quota in microseconds (0 to ignore, -1 to reset/unlimited).
     * @param cpu_period CPU period in microseconds (0 to ignore).
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     */
    virtual void updateCpuQuota(const std::string_view containerId, int64_t cpu_quota, int64_t cpu_period) const = 0;

    /**
     * @brief Fetch CPU, memory and network stats in a single API call.
     * @param containerId ID of the target container.
     * @return ContainerStats snapshot with CPU, memory, and network rates.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] virtual containers::ContainerStats getStats(const std::string_view containerId) = 0;

    /**
     * @brief Fetch the primary IP address of a running container (call once at setup).
     * @param containerId Docker container ID.
     * @return IPv4 address as a string.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] virtual std::string getContainerIp(const std::string_view containerId) const = 0;

    /**
     * @brief Open a file descriptor on the container's network namespace.
     *
     * The returned fd can be used with setns(CLONE_NEWNET) to enter the
     * container's network namespace from the host process. The caller
     * is responsible for closing the fd.
     *
     * @param containerId Target container ID.
     * @return A file descriptor opened on /proc/<pid>/ns/net.
     * @throws ContainerEngineError If the container cannot be inspected.
     */
    [[nodiscard]] virtual int getContainerNetnsFd(const std::string_view containerId) const = 0;

    /**
     * @brief Get the host PID of the container's init process.
     * @param containerId Docker container ID.
     * @return Host PID of the container's main process.
     * @throws ContainerEngineError If the container cannot be inspected.
     */
    [[nodiscard]] virtual int getContainerPid(const std::string_view containerId) const = 0;

    /**
     * @brief Retrieve system-level information from the container engine.
     * @return SystemInfo containing total memory and other host-level details.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] virtual SystemInfo getSystemInfo() const = 0;
};

}  // namespace chaos::orchestrator::containers
