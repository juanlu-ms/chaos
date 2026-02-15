#include <spdlog/spdlog.h>

#include "adapters/infra/docker/DockerClientAdapter.hpp"
#include "adapters/ui/cli/CliAdapter.hpp"
#include "domain/ports/IUserInterface.hpp"

using chaos::adapters::infra::docker::DockerClientAdapter;
using chaos::adapters::ui::cli::CliAdapter;

/**
 * @brief Application entry point.
 * @return Exit code (0 on success, 1 on failure).
 */
int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::warn);

    auto engine = DockerClientAdapter::create();
    std::shared_ptr<chaos::domain::ports::IContainerEngine> sharedEngine(std::move(engine));

    std::unique_ptr<chaos::domain::ports::IUserInterface> ui =
        std::make_unique<CliAdapter>(sharedEngine);
    return ui->run(argc, argv);
}
