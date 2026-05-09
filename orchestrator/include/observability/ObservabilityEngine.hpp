/**
 * @file ObservabilityEngine.hpp
 * @brief Engine for inspecting container state and retrieving logs.
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "../containers/IContainerEngine.hpp"
#include "../shared/TargetState.hpp"

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
    shared::TargetState observe(const std::string_view containerId) const;

    /**
     * @brief Fetch the IP. Called once at run setup, not on every observe tick.
     */
    std::optional<std::string> getContainerIp(const std::string_view containerId) const;

    // ── Data-source getters ─────────────────────────────────────────
    shared::ContainerStatus getStatus(const std::string_view containerId) const;
    std::string getLogs(const std::string_view containerId) const;
    containers::ContainerStats getStats(const std::string_view containerId) const;

private:
    std::shared_ptr<containers::IContainerEngine> engine_;
};

}  // namespace chaos::orchestrator::observability
