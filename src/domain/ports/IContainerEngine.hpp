#pragma once

#include <vector>

#include "domain/entities/Container.hpp"

namespace chaos::domain::ports {

using chaos::domain::Container;

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
     * @brief Kill a container by ID.
     * @param containerId Docker container ID.
     * @throws std::exception On transport errors or non-OK responses.
     */
    virtual void killContainer(const std::string& containerId) = 0;
};

}  // namespace chaos::domain::ports
