/**
 * @file ObservabilityEngine.cpp
 * @brief Implementation of container state and log observation.
 */

#include "observability/ObservabilityEngine.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <future>
#include <string>
#include <utility>

namespace chaos::orchestrator::observability {

ObservabilityEngine::ObservabilityEngine(std::shared_ptr<containers::IContainerEngine> engine)
    : engine_(std::move(engine)) {}

shared::TargetState ObservabilityEngine::observe(const std::string_view containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: observing container '{}'", containerId);

    shared::TargetState state;
    state.container_id = std::string(containerId);

    // Run the three independent data sources concurrently so that one tick
    // takes max(latency) instead of sum(latency).
    auto logsFut  = std::async(std::launch::async, [this, &containerId]() { return getLogs(containerId); });
    auto cpuFut   = std::async(std::launch::async, [this, &containerId]() { return getCpuUsage(containerId); });

    // Status is lightweight (cached inspect) — run on the calling thread
    // so we can decide whether to fetch the remaining metrics.
    state.status = getStatus(containerId);

    // Parse logs (already arriving in the background).
    {
        const std::string rawLogs = logsFut.get();
        state.recent_logs.clear();
        if (!rawLogs.empty()) {
            size_t start = 0;
            while (start < rawLogs.size()) {
                size_t end = rawLogs.find('\n', start);
                if (end == std::string::npos) end = rawLogs.size();
                std::string line = rawLogs.substr(start, end - start);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) {
                    state.recent_logs.push_back(std::move(line));
                }
                start = end + 1;
            }
        }
    }

    if (state.status != shared::ContainerStatus::Running) {
        SPDLOG_DEBUG("Container '{}' is not running. Skipping resource usage metrics.", containerId);
        state.cpu_usage_percent = std::nullopt;
        state.memory_usage_mb = std::nullopt;
    } else {
        // CPU was fetched in parallel; memory and network reuse the same
        // stats response via the DockerClient cache.
        state.cpu_usage_percent = cpuFut.get();
        state.memory_usage_mb = getMemoryUsage(containerId);
        try {
            auto [rx, tx] = getNetworkBps(containerId);
            state.network_rx_bps = rx;
            state.network_tx_bps = tx;
        } catch (const containers::ContainerEngineError& e) {
            SPDLOG_ERROR("ObservabilityEngine: error fetching network I/O for '{}': {}", containerId, e.what());
            state.network_rx_bps = std::nullopt;
            state.network_tx_bps = std::nullopt;
        }
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

std::pair<double, double> ObservabilityEngine::getNetworkBps(const std::string_view containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: getting network I/O for container '{}'", containerId);
    return engine_->getContainerNetworkBps(containerId);
}

}  // namespace chaos::orchestrator::observability
