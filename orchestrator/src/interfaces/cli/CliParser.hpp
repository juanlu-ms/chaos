/**
 * @file CliParser.hpp
 * @brief Command-line interface adapter for the chaos orchestrator.
 */

#pragma once

#include <chrono>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "containers/IContainerEngine.hpp"
#include "core/ChaosRunner.hpp"

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
    explicit CliParser(std::shared_ptr<containers::IContainerEngine> engine);
    ~CliParser() = default;

    CliParser(const CliParser&) = delete;
    CliParser& operator=(const CliParser&) = delete;

    /**
     * @brief Parse arguments and execute the requested command.
     * @param argv Argument values from main(), including executable at index 0.
     * @return Exit code (0 on success, non-zero on error).
     */
    int run(std::span<char*> argv) const;

private:
    std::shared_ptr<containers::IContainerEngine> m_engine;
    mutable core::ChaosRunner runner_;

    void printUsage() const;
    int handleList() const;
    int handleStop(const std::string& containerId) const;
    int handleKill(const std::string& containerId) const;
    int handleServe(int port) const;
    int handleRun(const std::string& manifestPath) const;
    int dispatchCommand(const std::vector<std::string>& args) const;

    [[nodiscard]] shared::TargetState runPerturbationsLoop(
        std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations, const std::string& targetId,
        std::chrono::seconds duration) const;
};

}  // namespace chaos::orchestrator::interfaces::cli
