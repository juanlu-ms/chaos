/**
 * @file ObservabilityEngine.cpp
 * @brief Implementation of container state and log observation.
 */

#include "observability/ObservabilityEngine.hpp"

#include <spdlog/spdlog.h>

#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace chaos::orchestrator::observability {

ObservabilityEngine::ObservabilityEngine(std::shared_ptr<containers::IContainerEngine> engine)
    : engine_(std::move(engine)) {}

core::TargetState ObservabilityEngine::observe(const std::string_view containerId) {
    SPDLOG_DEBUG("ObservabilityEngine: observing container '{}'", containerId);

    core::TargetState state;
    state.container_id = std::string(containerId);

    state.status = getStatus(containerId);
    auto logs = getLogs(containerId);
    parseLogLines(logs, state.recent_logs);

    if (state.status != containers::ContainerStatus::Running) {
        SPDLOG_DEBUG("Container '{}' is not running. Skipping resource usage metrics.", containerId);
    } else {
        // All resource metrics come from a single stats API call.
        try {
            auto stats = getStats(containerId);
            state.cpu_usage_percent = stats.cpu_percent;
            state.memory_usage_mb = stats.memory_mb;
            state.network_rx_bps = stats.network_rx_bps;
            state.network_tx_bps = stats.network_tx_bps;
        } catch (const containers::ContainerEngineError& e) {
            SPDLOG_ERROR("ObservabilityEngine: error fetching stats for '{}': {}", containerId, e.what());
        }
    }

    return state;
}

containers::ContainerStatus ObservabilityEngine::getStatus(const std::string_view containerId) const {
    return engine_->getStatus(containerId);
}

std::string ObservabilityEngine::getLogs(const std::string_view containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: getting logs for container '{}'", containerId);
    return engine_->getLogs(containerId);
}

containers::ContainerStats ObservabilityEngine::getStats(const std::string_view containerId) {
    SPDLOG_DEBUG("ObservabilityEngine: getting stats for container '{}'", containerId);
    return engine_->getStats(containerId);
}

void parseLogLines(const std::string_view raw, std::vector<std::string>& out) {
    out.clear();
    for (auto&& part : raw | std::views::split('\n')) {
        std::string_view line(part);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (!line.empty()) {
            out.emplace_back(line);
        }
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
