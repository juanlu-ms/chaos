#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Container.hpp"

namespace chaos::orchestrator::containers {

class ContainerEngineError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ContainerEngineTransportError : public ContainerEngineError {
public:
    using ContainerEngineError::ContainerEngineError;
};

class ContainerEngineApiError : public ContainerEngineError {
public:
    using ContainerEngineError::ContainerEngineError;
};

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
    virtual std::vector<Container> listContainers() = 0;

    /**
     * @brief Create a new container with the specified image and options.
     * @param image Container image to use (e.g. "nginx:latest").
     * @param options Additional options for container creation (e.g. env vars).
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void createContainer(const std::string_view image, const std::vector<std::string>& options) = 0;

    /**
     * @brief Start a container by ID.
     * @param containerId Docker container ID.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void startContainer(const std::string_view containerId) = 0;

    /**
     * @brief Stop a container by ID.
     * @param containerId Container ID to stop.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void stopContainer(const std::string_view containerId) = 0;

    /**
     * @brief Kill a container by ID.
     * @param containerId Docker container ID.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void killContainer(const std::string_view containerId) = 0;

    /**
     * @brief Execute a command inside a running container.
     * @param containerId ID of the target container.
     * @param command Command to execute (e.g. "ls -la /").
     * @return Output of the command execution.
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual std::string exec(const std::string_view containerId, const std::string_view command) = 0;

    /**
     * @brief Fetch stdout/stderr logs for a container.
     * @param containerId Docker container ID.
     * @return Raw log text (stdout/stderr combined).
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual std::string getLogs(const std::string_view containerId) = 0;

    /**
     * @brief Update container resources like CPU and memory.
     * @param containerId ID of the target container.
     * @param memory_bytes Max memory in bytes (0 to ignore/reset).
     * @param cpu_quota CPU quota in microseconds (0 to ignore/reset).
     * @param cpu_period CPU period in microseconds (0 to ignore).
     * @throws ContainerEngineError On transport errors or non-OK responses.
     */
    virtual void updateResources(const std::string_view containerId, int64_t memory_bytes, int64_t cpu_quota, int64_t cpu_period) = 0;

    /**
     * @brief Fetch the primary IP address of a running container.
     * @param containerId Docker container ID.
     * @return IPv4 address as a string.
     * @throws ContainerEngineError On transport errors, parsing failures, or if container is not running.
     */
    virtual std::string getContainerIp(const std::string_view containerId) = 0;
};

}  // namespace chaos::orchestrator::containers
