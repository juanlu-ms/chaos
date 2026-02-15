#include <spdlog/spdlog.h>

#include "adapters/infra/docker/DockerClientAdapter.hpp"

using chaos::adapters::infra::docker::DockerClientAdapter;

int main() {
    try {
        auto client = DockerClientAdapter::create();
        auto containers = client->listContainers();

        spdlog::info("Containers found: {}", containers.size());
        for (const auto& container : containers) {
            spdlog::info("  {} {} {}", container.id.substr(0, 12), container.name, container.state);
        }
    } catch (const std::exception& ex) {
        spdlog::error("Error: {}", ex.what());
        return 1;
    }
    return 0;
}
