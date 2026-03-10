#include <spdlog/spdlog.h>

#include <containers/internal/DockerClient.hpp>
#include <interfaces/cli/CliParser.hpp>

using chaos::orchestrator::containers::internal::DockerClient;
using chaos::orchestrator::interfaces::cli::CliParser;

/**
 * @brief Orchestrator entry point.
 * @return Exit code (0 on success, 1 on failure).
 */
int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::warn);

    auto engine = DockerClient::create();

    CliParser ui(*engine);
    return ui.run(argc, argv);
}
