/**
 * @file ObservabilityEngine.cpp
 * @brief Implementation of container state and log observation.
 */

#include "observability/ObservabilityEngine.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace chaos::orchestrator::observability {

ObservabilityEngine::ObservabilityEngine(std::shared_ptr<containers::IContainerEngine> engine)
    : engine_(std::move(engine)) {}

shared::TargetState ObservabilityEngine::observe(const std::string_view containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: observing container '{}'", containerId);

    shared::TargetState state;
    state.container_id = std::string(containerId);

    state.status = getStatus(containerId);
    state.recent_logs = {getLogs(containerId)};

    if (state.status != shared::ContainerStatus::Running) {
        SPDLOG_DEBUG("Container '{}' is not running. Skipping resource usage metrics.", containerId);
        state.cpu_usage_percent = std::nullopt;
        state.memory_usage_mb = std::nullopt;
        state.container_ip = std::nullopt;
    } else {
        state.cpu_usage_percent = getCpuUsage(containerId);
        state.memory_usage_mb = getMemoryUsage(containerId);
        state.container_ip = getContainerIp(containerId);
    }

    return state;
}

shared::ContainerStatus ObservabilityEngine::getStatus(const std::string_view containerId) const {
    return engine_->getStatus(containerId);
}

std::string ObservabilityEngine::getLogs(const std::string_view containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: getting logs for container '{}'", containerId);
    return engine_->getLogs(containerId);
}

std::optional<double> ObservabilityEngine::getMemoryUsage(const std::string_view containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: getting memory usage for container '{}'", containerId);
    try {
        return engine_->getContainerMemoryUsage(containerId);
    } catch (const containers::ContainerEngineError& e) {
        SPDLOG_ERROR("ObservabilityEngine: error occurred while fetching memory usage for container '{}': {}",
                     containerId, e.what());
        return std::nullopt;
    }
}

std::optional<double> ObservabilityEngine::getCpuUsage(const std::string_view containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: getting CPU usage for container '{}'", containerId);
    try {
        return engine_->getContainerCpuUsage(containerId);
    } catch (const containers::ContainerEngineError& e) {
        SPDLOG_ERROR("ObservabilityEngine: error occurred while fetching CPU usage for container '{}': {}", containerId,
                     e.what());
        return std::nullopt;
    }
}

std::optional<std::string> ObservabilityEngine::getContainerIp(const std::string_view containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: getting IP for container '{}'", containerId);
    try {
        return engine_->getContainerIp(containerId);
    } catch (const containers::ContainerEngineError& e) {
        SPDLOG_ERROR("ObservabilityEngine: error occurred while fetching IP for container '{}': {}", containerId,
                     e.what());
        return std::nullopt;
    }
}

}  // namespace chaos::orchestrator::observability
