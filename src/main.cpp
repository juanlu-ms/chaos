#include <spdlog/spdlog.h>

#include "adapters/infra/docker/DockerClientAdapter.hpp"

using chaos::adapters::infra::docker::DockerClientAdapter;

int main() {
    spdlog::set_level(spdlog::level::debug);
    try {
        auto client = DockerClientAdapter::create();
        auto containers = client->listContainers();

        for (const auto& container : containers) {
            spdlog::info("  {} {} {}", container.id.substr(0, 12), container.name, container.state);
        }
    } catch (const std::exception& ex) {
        spdlog::error("Error: {}", ex.what());
        return 1;
    }
    return 0;
}
