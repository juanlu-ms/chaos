/**
 * @file IContainerEngine.hpp
 * @brief Port for container engine operations and related exceptions.
 */

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Container.hpp"
#include "SystemInfo.hpp"
#include "shared/ContainerStatus.hpp"

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
     * @brief Create a new container with the specified image and options.
     * @param image Container image to use (e.g. "nginx:latest").
     * @param options Additional options for container creation (e.g. env vars).
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void createContainer(const std::string_view image, const std::vector<std::string>& options) const = 0;

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
     * @brief Execute a command inside a running container.
     * @param containerId ID of the target container.
     * @param command Command to execute (e.g. "ls -la /").
     * @return Output of the command execution.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    [[nodiscard]] virtual std::string exec(const std::string_view containerId, const std::string_view command) const = 0;

    /**
     * @brief Get status of a container by ID.
     * @param containerId Docker container ID.
     * @return ContainerStatus enum value representing the container's state.
     * @throws ContainerEngineApiError On non-OK HTTP responses.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] virtual shared::ContainerStatus getStatus(const std::string_view containerId) const = 0;

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
     * @brief Fetch the memory usage of a running container in MB.
     * @param containerId Docker container ID.
     * @return Memory usage in MB.
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On unexpected response format.
     */
    [[nodiscard]] virtual double getContainerMemoryUsage(const std::string_view containerId) const = 0;

    /**
     * @brief Fetch the CPU core limit configured for a running container.
     * @param containerId Docker container ID.
     * @return Configured CPU core limit (supports decimals, e.g. 0.5, 1.25, 2.0).
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On unexpected response format.
     */
    [[nodiscard]] virtual double getContainerCpuUsage(const std::string_view containerId) const = 0;

    /**
     * @brief Fetch the primary IP address of a running container.
     * @param containerId Docker container ID.
     * @return IPv4 address as a string.
     * @throws std::invalid_argument On empty containerId.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On unexpected response format.
     */
    [[nodiscard]] virtual std::string getContainerIp(const std::string_view containerId) const = 0;

    /**
     * @brief Retrieve system-level information from the container engine.
     * @return SystemInfo containing total memory and other host-level details.
     * @throws ContainerEngineApiError On non-200 HTTP response.
     * @throws ContainerEngineParseError On JSON parsing failures.
     */
    [[nodiscard]] virtual SystemInfo getSystemInfo() const = 0;
};

}  // namespace chaos::orchestrator::containers
