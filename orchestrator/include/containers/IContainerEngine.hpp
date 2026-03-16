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
};

}  // namespace chaos::orchestrator::containers
