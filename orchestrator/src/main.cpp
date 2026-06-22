/**
 * @file main.cpp
 * @brief Entry point for the chaos orchestrator.
 */

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <charconv>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>

#include "containers/ContainerEngineFactory.hpp"
#include "history/FileRunHistory.hpp"
#include "history/IRunHistory.hpp"
#include "interfaces/cli/CliParser.hpp"
#include "interfaces/cli/ProgressCoordinator.hpp"
#include "interfaces/web/Server.hpp"
#include "observability/LoggerSetup.hpp"

using chaos::orchestrator::containers::createContainerEngine;

namespace {

std::filesystem::path resolveHistoryDir() {
    if (const char* env = std::getenv("CHAOS_HISTORY_DIR")) {
        return std::filesystem::path(env);
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".chaos" / "history";
    }
    return std::filesystem::temp_directory_path() / "chaos_history";
}

size_t resolveHistoryMax() {
    if (const char* env = std::getenv("CHAOS_HISTORY_MAX")) {
        int val = std::atoi(env);
        if (val > 0) return static_cast<size_t>(val);
    }
    return 50;
}

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

int runServer(std::shared_ptr<chaos::orchestrator::containers::IContainerEngine> engine,
              std::shared_ptr<chaos::orchestrator::history::IRunHistory> history, std::span<char*> args) {
    int port = parsePort(args);
    if (port < 0) {
        return 1;
    }

    try {
        chaos::orchestrator::interfaces::web::Server server(std::move(engine), std::move(history));
        server.listen(port);
    } catch (const std::system_error& ex) {
        SPDLOG_ERROR("Server error: {}", ex.what());
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Parse global logging/color flags before emitting any log lines.
    auto parsed =
        chaos::orchestrator::interfaces::cli::parseGlobalFlags(std::span<char*>{argv, static_cast<std::size_t>(argc)});
    if (parsed.error.has_value()) {
        fmt::print(stderr, "Error: {}\n", parsed.error.value());
        return 1;
    }
    chaos::orchestrator::observability::initLogger(parsed.options.logLevel, parsed.options.colorize);

    auto engine = createContainerEngine();
    auto history =
        std::make_shared<chaos::orchestrator::history::FileRunHistory>(resolveHistoryDir(), resolveHistoryMax());

    SPDLOG_INFO("chaos starting");

    // Route "serve" directly to the web server, bypassing CLI parser
    if (parsed.args.size() >= 2 && isServeCommand(parsed.args[1])) {
        return runServer(engine, history, std::span<char*>{parsed.args.data() + 2, parsed.args.size() - 2});
    }

    chaos::orchestrator::interfaces::cli::installProgressAwareSink();

    // All other commands go through the CLI parser
    chaos::orchestrator::interfaces::cli::CliParser cli(engine, history);
    return cli.run(std::span<char*>{parsed.args.data(), parsed.args.size()});
}
