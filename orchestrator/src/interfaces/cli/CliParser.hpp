#pragma once

#include <containers/IContainerEngine.hpp>
#include <string>

namespace chaos::orchestrator::interfaces::cli {

/**
 * @brief CLI adapter for the chaos orchestrator.
 */
class CliParser {
public:
    /**
     * @brief Construct the CLI adapter with a container engine.
     * @param engine Non-owning reference to the container engine port.
     */
    explicit CliParser(chaos::orchestrator::containers::IContainerEngine& engine);
    ~CliParser() = default;

    CliParser(const CliParser&) = delete;
    CliParser& operator=(const CliParser&) = delete;

    /**
     * @brief Parse arguments and execute the requested command.
     * @param argc Argument count from main().
     * @param argv Argument values from main().
     * @return Exit code (0 on success, non-zero on error).
     */
    int run(int argc, char* argv[]) const;

private:
    chaos::orchestrator::containers::IContainerEngine& m_engine;

    void printUsage() const;
    int handleList() const;
    int handleStop(const std::string& containerId) const;
    int handleKill(const std::string& containerId) const;
    int handleServe(int port) const;
    int handleRun(const std::string& manifestPath) const;
};

}  // namespace chaos::orchestrator::interfaces::cli
