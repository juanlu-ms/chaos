/**
 * @file ObservabilityEngine.hpp
 * @brief Engine for inspecting container state and retrieving logs.
 */

#pragma once

#include <memory>
#include <string>

#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::observability {

/**
 * @brief Provides container observation primitives for the validation layer.
 *
 * Wraps an IContainerEngine to answer high-level questions:
 * - Is a container currently running?
 * - What are its latest logs?
 */
class ObservabilityEngine {
public:
    /**
     * @brief Construct a new ObservabilityEngine.
     * @param engine Container engine instance to wrap.
     */
    explicit ObservabilityEngine(std::shared_ptr<containers::IContainerEngine> engine);

    /**
     * @brief Check whether a container is currently in the running state.
     * @param containerId Docker container ID or name.
     * @return true if the container exists and its state is "running".
     */
    bool isRunning(const std::string& containerId) const;

    /**
     * @brief Retrieve stdout/stderr logs for a container.
     * @param containerId Docker container ID.
     * @return Raw log text.
     * @throws containers::ContainerEngineError On retrieval failure.
     */
    std::string getLogs(const std::string& containerId) const;

    /**
     * @brief Retrieve the IPv4 address for a container.
     * @param containerId Docker container ID.
     * @return IPv4 address as a string.
     * @throws containers::ContainerEngineError On retrieval failure.
     */
    std::string getContainerIp(const std::string& containerId) const;

private:
    std::shared_ptr<containers::IContainerEngine> engine_;
};

}  // namespace chaos::orchestrator::observability
