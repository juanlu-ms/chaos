#include "adapters/ui/cli/CliAdapter.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <adapters/ui/web/Server.hpp>
#include <string>

namespace chaos::adapters::ui::cli {

CliAdapter::CliAdapter(chaos::domain::ports::IContainerEngine& engine) : m_engine(engine) {}

int CliAdapter::run(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Scan for global flags before dispatching
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            spdlog::set_level(spdlog::level::debug);
        }
    }

    const std::string command = argv[1];

    if (command == "--verbose" || command == "-v") {
        if (argc < 3) {
            printUsage(argv[0]);
            return 1;
        }
        // Re-run with the flag consumed (shift args)
        return run(argc - 1, argv + 1);
    }

    if (command == "help" || command == "--help" || command == "-h") {
        printUsage(argv[0]);
        return 0;
    }

    if (command == "list") {
        return handleList();
    }

    if (command == "stop") {
        if (argc < 3) {
            spdlog::error("'stop' requires a container ID");
            return 1;
        }
        return handleStop(argv[2]);
    }

    if (command == "kill") {
        if (argc < 3) {
            spdlog::error("'kill' requires a container ID");
            return 1;
        }
        return handleKill(argv[2]);
    }

    if (command == "serve") {
        int port = 8080;
        for (int i = 2; i < argc - 1; ++i) {
            if (std::string(argv[i]) == "--port") {
                try {
                    port = std::stoi(argv[i + 1]);
                } catch (...) {
                    spdlog::error("Invalid port number: {}", argv[i + 1]);
                    return 1;
                }
            }
        }
        return handleServe(port);
    }

    spdlog::error("Unknown command: '{}'", command);
    printUsage(argv[0]);
    return 1;
}

void CliAdapter::printUsage(const std::string& programName) const {
    fmt::print(
        "Usage: {} <command> [options]\n"
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
        "  -v, --verbose         Enable debug logging\n",
        programName);
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
                c.id.empty()
                    ? std::string("<missing>")
                    : (c.id.size() > 12 ? c.id.substr(0, 12) : c.id);
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
