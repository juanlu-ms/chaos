/// @file ObservabilityEngine.cpp
/// @brief Implementation of container state and log observation.

#include "observability/ObservabilityEngine.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace chaos::orchestrator::observability {

ObservabilityEngine::ObservabilityEngine(std::shared_ptr<containers::IContainerEngine> engine)
    : engine_(std::move(engine)) {}

bool ObservabilityEngine::isRunning(const std::string& containerId) const {
    const auto containers = engine_->listContainers();
    const auto it = std::find_if(containers.begin(), containers.end(), [&containerId](const auto& c) {
        if (c.id == containerId || c.id.starts_with(containerId)) {
            return c.state == "running";
        }
        if (c.name == containerId || c.name == "/" + containerId) {
            return c.state == "running";
        }
        return false;
    });
    const bool running = (it != containers.end());
    SPDLOG_DEBUG("Container '{}' isRunning: {}", containerId, running);
    return running;
}

std::string ObservabilityEngine::getLogs(const std::string& containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: getting logs for container '{}'", containerId);
    return engine_->getLogs(containerId);
}

std::string ObservabilityEngine::getContainerIp(const std::string& containerId) const {
    SPDLOG_DEBUG("ObservabilityEngine: getting IP for container '{}'", containerId);
    return engine_->getContainerIp(containerId);
}

}  // namespace chaos::orchestrator::observability
