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

core::TargetState ObservabilityEngine::observe(const std::string_view container_id) {
    SPDLOG_DEBUG("ObservabilityEngine: observing container '{}'", container_id);

    core::TargetState state;
    state.container_id = std::string(container_id);

    state.status = getStatus(container_id);
    auto logs = getLogs(container_id);
    parseLogLines(logs, state.recent_logs);

    if (state.status != containers::ContainerStatus::Running) {
        SPDLOG_DEBUG("Container '{}' is not running. Skipping resource usage metrics.", container_id);
    } else {
        // All resource metrics come from a single stats API call.
        try {
            auto stats = getStats(container_id);
            state.cpu_usage_percent = stats.cpu_percent;
            state.memory_usage_mb = stats.memory_mb;
            state.network_rx_bps = stats.network_rx_bps;
            state.network_tx_bps = stats.network_tx_bps;
        } catch (const containers::ContainerEngineError& e) {
            SPDLOG_ERROR("ObservabilityEngine: error fetching stats for '{}': {}", container_id, e.what());
        }
    }

    return state;
}

containers::ContainerStatus ObservabilityEngine::getStatus(const std::string_view container_id) const {
    return engine_->getStatus(container_id);
}

std::string ObservabilityEngine::getLogs(const std::string_view container_id) const {
    return engine_->getLogs(container_id);
}

containers::ContainerStats ObservabilityEngine::getStats(const std::string_view container_id) {
    return engine_->getStats(container_id);
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

std::optional<std::string> ObservabilityEngine::getContainerIp(const std::string_view container_id) const {
    try {
        return engine_->getContainerIp(container_id);
    } catch (const containers::ContainerEngineError& e) {
        SPDLOG_ERROR("ObservabilityEngine: error occurred while fetching IP for container '{}': {}", container_id,
                     e.what());
        return std::nullopt;
    }
}

}  // namespace chaos::orchestrator::observability
