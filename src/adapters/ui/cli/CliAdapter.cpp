#include "adapters/ui/cli/CliAdapter.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <adapters/ui/web/Server.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace chaos::adapters::ui::cli {

CliAdapter::CliAdapter(chaos::domain::ports::IContainerEngine& engine) : m_engine(engine) {}

int CliAdapter::run(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    // Scan for global flags and build a filtered argument list without them
    std::vector<std::string> args;
    args.emplace_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            spdlog::set_level(spdlog::level::debug);
        } else {
            args.push_back(arg);
        }
    }

    if (args.size() < 2) {
        printUsage();
        return 1;
    }

    const std::string& command = args[1];

    if (command == "help" || command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }

    if (command == "list") {
        return handleList();
    }

    if (command == "stop") {
        if (args.size() < 3) {
            spdlog::error("'stop' requires a container ID");
            return 1;
        }
        return handleStop(args[2]);
    }

    if (command == "kill") {
        if (args.size() < 3) {
            spdlog::error("'kill' requires a container ID");
            return 1;
        }
        return handleKill(args[2]);
    }

    if (command == "serve") {
        int port = 8080;
        for (size_t i = 2; i + 1 < args.size(); ++i) {
            if (args[i] == "--port") {
                try {
                    port = std::stoi(args[i + 1]);
                } catch (...) {
                    spdlog::error("Invalid port number: {}", args[i + 1]);
                    return 1;
                }
            }
        }
        return handleServe(port);
    }

    spdlog::error("Unknown command: '{}'", command);
    printUsage();
    return 1;
}

void CliAdapter::printUsage() const {
    fmt::print(
        "Usage: chaos <command> [options]\n"
        "\n"
        "Chaos — Resilience Tool for Docker Containers\n"
        "\n"
        "Commands:\n"
        "  list                  List all containers\n"
        "  stop  <container_id>  Stop a running container\n"
        "  kill  <container_id>  Kill a running container\n"
        "  serve [--port <n>]    Start the web server (default: 8080)\n"
        "  help                  Show this help message\n"
        "\n"
        "Options:\n"
        "  -v, --verbose         Enable debug logging\n");
}

int CliAdapter::handleList() const {
    try {
        auto containers = m_engine.listContainers();
        if (containers.empty()) {
            fmt::print("No containers found.\n");
            return 0;
        }
        fmt::print("{:<14} {:<30} {}\n", "CONTAINER ID", "NAME", "STATE");
        for (const auto& c : containers) {
            const std::string displayId =
                c.id.empty() ? std::string("<missing>") : (c.id.size() > 12 ? c.id.substr(0, 12) : c.id);
            fmt::print("{:<14} {:<30} {}\n", displayId, c.name, c.state);
        }
    } catch (const std::exception& ex) {
        spdlog::error("Failed to list containers: {}", ex.what());
        return 1;
    }
    return 0;
}

int CliAdapter::handleStop(const std::string& containerId) const {
    try {
        m_engine.stopContainer(containerId);
        fmt::print("Container {} stopped.\n", containerId);
    } catch (const std::exception& ex) {
        spdlog::error("Failed to stop container {}: {}", containerId, ex.what());
        return 1;
    }
    return 0;
}

int CliAdapter::handleKill(const std::string& containerId) const {
    try {
        m_engine.killContainer(containerId);
        fmt::print("Container {} killed.\n", containerId);
    } catch (const std::exception& ex) {
        spdlog::error("Failed to kill container {}: {}", containerId, ex.what());
        return 1;
    }
    return 0;
}

int CliAdapter::handleServe(int port) const {
    try {
        auto server = chaos::adapters::ui::web::Server(m_engine);
        server.listen(port);
    } catch (const std::exception& ex) {
        spdlog::error("Server error: {}", ex.what());
        return 1;
    }
    return 0;
}

}  // namespace chaos::adapters::ui::cli
