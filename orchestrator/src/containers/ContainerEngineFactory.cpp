#include "containers/ContainerEngineFactory.hpp"

#include <memory>

#include "internal/DockerClient.hpp"

namespace chaos::orchestrator::containers {

std::shared_ptr<IContainerEngine> createContainerEngine(const std::string& socket_path) {
    return containers::DockerClient::create(socket_path);
}

}  // namespace chaos::orchestrator::containers
