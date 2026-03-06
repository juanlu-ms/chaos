#include <spdlog/spdlog.h>

#include "adapters/infra/docker/DockerClientAdapter.hpp"
#include "adapters/ui/cli/CliAdapter.hpp"

using chaos::adapters::infra::docker::DockerClientAdapter;
using chaos::adapters::ui::cli::CliAdapter;

/**
 * @brief Application entry point.
 * @return Exit code (0 on success, 1 on failure).
 */
int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::warn);

    auto engine = DockerClientAdapter::create();

    CliAdapter ui(*engine);
    return ui.run(argc, argv);
}
