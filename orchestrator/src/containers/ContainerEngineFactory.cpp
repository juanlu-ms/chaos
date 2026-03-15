#include <containers/ContainerEngineFactory.hpp>
#include <containers/internal/DockerClient.hpp>

namespace chaos::orchestrator::containers {

std::unique_ptr<IContainerEngine> createContainerEngine(const std::string& socketPath) {
    return chaos::orchestrator::containers::internal::DockerClient::create(socketPath);
}

}  // namespace chaos::orchestrator::containers
