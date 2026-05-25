/**
 * @file main.cpp
 * @brief Entry point for the chaos orchestrator.
 */

#include <spdlog/spdlog.h>

#include <span>

#include "containers/ContainerEngineFactory.hpp"
#include "interfaces/cli/CliParser.hpp"

using chaos::orchestrator::containers::createContainerEngine;
using chaos::orchestrator::interfaces::cli::CliParser;

/**
 * @brief Orchestrator entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit code (0 on success, 1 on failure).
 */
int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);

    auto engine = createContainerEngine();

    CliParser cli(engine);
    return cli.run(std::span<char*>{argv, static_cast<std::size_t>(argc)});
}
