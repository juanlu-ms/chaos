#pragma once

#include <string_view>
#include <vector>

#include "Container.hpp"

namespace chaos::orchestrator::containers {

/**
 * @brief Port for container engine operations.
 */
class IContainerEngine {
public:
    virtual ~IContainerEngine() = default;

    /**
     * @brief List containers available in the engine.
     * @return Vector of container summaries.
     * @throws std::exception On transport or parsing failures.
     */
    virtual std::vector<Container> listContainers() = 0;

    /**
     * @brief Create a new container with the specified image and options.
     * @param image Container image to use (e.g. "nginx:latest").
     * @param options Additional options for container creation (e.g. env vars).
     * @return ID of the created container.
     * @throws std::exception On transport errors or non-OK responses.
     */
    virtual void createContainer(const std::string_view image, const std::vector<std::string>& options) = 0;

    /**
     * @brief Stop a container by ID.
     * @param containerId Container ID to stop.
     * @throws std::exception On transport errors or non-OK responses.
     */
    virtual void stopContainer(const std::string_view containerId) = 0;

    /**
     * @brief Kill a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    virtual void killContainer(const std::string_view containerId) = 0;

    /**
     * @brief Execute a command inside a running container.
     * @param containerId ID of the target container.
     * @param command Command to execute (e.g. "ls -la /").
     * @return Output of the command execution.
     * @throws std::exception On transport errors or non-OK responses.
     */
    virtual std::string exec(const std::string_view containerId, const std::string_view command) = 0;
};

}  // namespace chaos::orchestrator::containers
