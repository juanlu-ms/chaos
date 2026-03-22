#pragma once

#include <memory>
#include <string>

#include "IContainerEngine.hpp"

namespace chaos::orchestrator::containers {

/**
 * @brief Create the default container engine implementation.
 * @param socketPath Docker Engine Unix socket path.
 * @return Container engine instance owned by the caller.
 */
std::shared_ptr<IContainerEngine> createContainerEngine(const std::string& socketPath = "/var/run/docker.sock");

}  // namespace chaos::orchestrator::containers
