/**
 * @file main.cpp
 * @brief Entry point for the chaos orchestrator.
 */

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <charconv>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>

#include "containers/ContainerEngineFactory.hpp"
#include "interfaces/cli/CliParser.hpp"
#include "interfaces/web/Server.hpp"

using chaos::orchestrator::containers::createContainerEngine;

namespace {

[[nodiscard]] bool isServeCommand(std::string_view arg) { return arg == "serve"; }

int parsePort(std::span<char*> args) {
    int port = 8080;
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (std::string_view(args[i]) == "--port") {
            auto [ptr, ec] = std::from_chars(args[i + 1], args[i + 1] + std::strlen(args[i + 1]), port);
            if (ec != std::errc{}) {
                fmt::print(stderr, "Invalid port number: {}\n", args[i + 1]);
                return -1;
            }
        }
    }
    return port;
}

int runServer(std::shared_ptr<chaos::orchestrator::containers::IContainerEngine> engine, std::span<char*> args) {
    int port = parsePort(args);
    if (port < 0) {
        return 1;
    }

    try {
        chaos::orchestrator::interfaces::web::Server server(std::move(engine));
        server.listen(port);
    } catch (const std::system_error& ex) {
        SPDLOG_ERROR("Server error: {}", ex.what());
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    auto engine = createContainerEngine();

    SPDLOG_INFO("chaos starting");

    std::span<char*> args{argv, static_cast<std::size_t>(argc)};

    // Route "serve" directly to the web server, bypassing CLI parser
    if (argc >= 2 && isServeCommand(argv[1])) {
        return runServer(engine, args.subspan(2));
    }

    // All other commands go through the CLI parser
    chaos::orchestrator::interfaces::cli::CliParser cli(engine);
    return cli.run(args);
}
