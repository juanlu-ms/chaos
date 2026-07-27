/**
 * @file main.cpp
 * @brief Entry point for the chaos orchestrator.
 */

#include <fmt/format.h>
#include <pwd.h>
#include <spdlog/spdlog.h>

#include <charconv>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include "containers/ContainerEngineFactory.hpp"
#include "history/FileRunHistory.hpp"
#include "history/IRunHistory.hpp"
#include "interfaces/cli/CliParser.hpp"
#include "interfaces/cli/CliSpec.hpp"
#include "interfaces/cli/ProgressCoordinator.hpp"
#include "interfaces/web/Server.hpp"
#include "observability/LoggerSetup.hpp"
#include "observability/OtlpRunObserver.hpp"

using chaos::orchestrator::containers::createContainerEngine;

namespace {

std::filesystem::path resolveHistoryDir() {
    if (const char* env = std::getenv("CHAOS_HISTORY_DIR")) {
        return std::filesystem::path(env);
    }
    if (const char* sudo_user = std::getenv("SUDO_USER")) {
        const char* home = std::getenv("HOME");
        if (!home || std::string_view(home) == "/root") {
            if (struct passwd* pw = getpwnam(sudo_user)) {
                if (pw->pw_dir && pw->pw_dir[0] != '\0') {
                    return std::filesystem::path(pw->pw_dir) / ".chaos" / "history";
                }
            }
        }
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

/** True when the command is executed by main() rather than by the CLI parser. */
[[nodiscard]] bool isEntryPointCommand(std::string_view arg) {
    const auto* spec = chaos::orchestrator::interfaces::cli::findCommand(arg);
    return spec != nullptr && spec->owner == chaos::orchestrator::interfaces::cli::CommandOwner::EntryPoint;
}

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
              std::shared_ptr<chaos::orchestrator::history::IRunHistory> history, std::span<char*> args,
              std::optional<std::string> otlp_endpoint) {
    int port = parsePort(args);
    if (port < 0) {
        return 1;
    }

    try {
        chaos::orchestrator::interfaces::web::Server server(std::move(engine), std::move(history),
                                                            std::move(otlp_endpoint));
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
    chaos::orchestrator::observability::initLogger(parsed.options.log_level, parsed.options.colorize);

    auto engine = createContainerEngine();
    auto history =
        std::make_shared<chaos::orchestrator::history::FileRunHistory>(resolveHistoryDir(), resolveHistoryMax());
    auto otlp_endpoint = chaos::orchestrator::observability::resolveOtlpEndpoint();

    SPDLOG_DEBUG("chaos starting");

    // Route entry-point commands ("serve") directly to the web server, bypassing CLI parser
    if (parsed.args.size() >= 2 && isEntryPointCommand(parsed.args[1])) {
        return runServer(engine, history, std::span<char*>{parsed.args.data() + 2, parsed.args.size() - 2},
                         otlp_endpoint);
    }

    chaos::orchestrator::interfaces::cli::installProgressAwareSink();

    // All other commands go through the CLI parser
    chaos::orchestrator::interfaces::cli::CliParser cli(engine, history, otlp_endpoint);
    return cli.run(std::span<char*>{parsed.args.data(), parsed.args.size()});
}
