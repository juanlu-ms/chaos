#include "containers/ContainerEngineFactory.hpp"

#include <memory>

#include "internal/DockerClient.hpp"

namespace chaos::orchestrator::containers {

std::shared_ptr<IContainerEngine> createContainerEngine(const std::string& socketPath) {
    return chaos::orchestrator::containers::internal::DockerClient::create(socketPath);
}

}  // namespace chaos::orchestrator::containers
