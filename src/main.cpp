#include <adapters/ui/web/Server.hpp>
#include <cstdio>

#include "adapters/infra/docker/DockerClientAdapter.hpp"
#include "domain/entities/Container.hpp"

using kaos::adapters::infra::docker::DockerClientAdapter;

int main() {
    auto client = DockerClientAdapter::create();
    auto containers = client.listContainers();

    std::printf("ID\tName\tState\n");
    for (const auto& container : containers) {
        std::printf("%s\t%s\t%s\n", container.id.c_str(), container.name.c_str(), container.state.c_str());
    }
    return 0;
}
