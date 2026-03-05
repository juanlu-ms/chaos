#pragma once

#include <domain/ports/IContainerEngine.hpp>
#include <domain/ports/IUserInterface.hpp>
#include <string>

namespace chaos::adapters::ui::cli {

/**
 * @brief CLI adapter for the chaos orchestrator.
 *
 * Parses command-line arguments and dispatches the appropriate
 * operation through the container engine port.
 *
 * Supported commands:
 *   list                  List all containers
 *   stop  <container_id>  Stop a container
 *   kill  <container_id>  Kill a container
 *   serve [--port <n>]    Start the web server (default port 8080)
 *   help                  Show usage information
 */
class CliAdapter : public chaos::domain::ports::IUserInterface {
public:
    /**
     * @brief Construct the CLI adapter with a container engine.
     * @param engine Non-owning reference to the container engine port.
     */
    explicit CliAdapter(chaos::domain::ports::IContainerEngine& engine);
    ~CliAdapter() override = default;

    CliAdapter(const CliAdapter&) = delete;
    CliAdapter& operator=(const CliAdapter&) = delete;

    /**
     * @brief Parse arguments and execute the requested command.
     * @param argc Argument count from main().
     * @param argv Argument values from main().
     * @return Exit code (0 on success, non-zero on error).
     */
    int run(int argc, char* argv[]) override;

private:
    chaos::domain::ports::IContainerEngine& m_engine;

    void printUsage() const;
    int handleList() const;
    int handleStop(const std::string& containerId) const;
    int handleKill(const std::string& containerId) const;
    int handleServe(int port) const;
};

}  // namespace chaos::adapters::ui::cli
