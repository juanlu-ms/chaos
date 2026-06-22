/**
 * @file ObservabilityEngine.hpp
 * @brief Engine for inspecting container state and retrieving logs.
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "containers/IContainerEngine.hpp"
#include "core/TargetState.hpp"

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
     * @brief Takes a complete snapshot of the container's current state.
     * This is the main entry point for the Validation engine to get its evidence.
     * @param containerId Docker container ID or name.
     * @return A populated TargetState DTO with all observed metrics.
     * @throws std::system_error On retrieval failure.
     */
    core::TargetState observe(const std::string_view containerId);

    /**
     * @brief Fetch the IP. Called once at run setup, not on every observe tick.
     * @param containerId Docker container ID or name.
     * @return The container's IP address, or std::nullopt if not available.
     */
    std::optional<std::string> getContainerIp(const std::string_view containerId) const;

    // ── Data-source getters ─────────────────────────────────────────

    /**
     * @brief Get the current container status.
     * @param containerId Docker container ID or name.
     * @return The container's status.
     */
    containers::ContainerStatus getStatus(const std::string_view containerId) const;

    /**
     * @brief Retrieve the full container log output.
     * @param containerId Docker container ID or name.
     * @return Raw log text from the container.
     */
    std::string getLogs(const std::string_view containerId) const;

    /**
     * @brief Get current container resource statistics.
     * @param containerId Docker container ID or name.
     * @return CPU, memory, and I/O statistics for the container.
     */
    containers::ContainerStats getStats(const std::string_view containerId);

private:
    std::shared_ptr<containers::IContainerEngine> engine_;
};

/**
 * @brief Split raw container log text into individual lines, stripping trailing CR.
 * @param raw Raw log text from the container engine.
 * @param out Output vector of individual log lines.
 */
void parseLogLines(std::string_view raw, std::vector<std::string>& out);

}  // namespace chaos::orchestrator::observability
