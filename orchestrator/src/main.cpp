#include <spdlog/spdlog.h>

#include <containers/ContainerEngineFactory.hpp>
#include <interfaces/cli/CliParser.hpp>
#include <span>

using chaos::orchestrator::containers::createContainerEngine;
using chaos::orchestrator::interfaces::cli::CliParser;

/**
 * @brief Orchestrator entry point.
 * @return Exit code (0 on success, 1 on failure).
 */
int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);

    auto engine = createContainerEngine();

    CliParser ui(engine);
    return ui.run(std::span<char*>{argv, static_cast<std::size_t>(argc)});
}
