/**
 * @file CliParser.hpp
 * @brief Command-line interface adapter for the chaos orchestrator.
 */

#pragma once

#include <containers/IContainerEngine.hpp>
#include <chrono>
#include <manifests/ManifestParser.hpp>
#include <memory>
#include <perturbations/PerturbationFactory.hpp>
#include <shared/StateBroadcaster.hpp>
#include <span>
#include <string>
#include <vector>

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
    /**
     * @brief The container engine instance.
     */
    std::shared_ptr<containers::IContainerEngine> m_engine;

    /**
     * @brief Print the usage message.
     */
    void printUsage() const;
    /**
     * @brief Handle the 'list' command.
     * @return Exit code (0 on success, non-zero on error).
     */
    int handleList() const;
    /**
     * @brief Handle the 'stop' command.
     * @param containerId The ID of the container to stop.
     * @return Exit code (0 on success, non-zero on error).
     */
    int handleStop(const std::string& containerId) const;
    /**
     * @brief Handle the 'kill' command.
     * @param containerId The ID of the container to kill.
     * @return Exit code (0 on success, non-zero on error).
     */
    int handleKill(const std::string& containerId) const;
    /**
     * @brief Handle the 'serve' command.
     * @param port The port to listen on.
     * @return Exit code (0 on success, non-zero on error).
     */
    int handleServe(int port) const;
    /**
     * @brief Handle the 'run' command.
     * @param manifestPath The path to the manifest JSON file.
     * @return Exit code (0 on success, non-zero on error).
     */
    int handleRun(const std::string& manifestPath) const;
    /**
     * @brief Handle the 'tui' command — launches the interactive terminal UI.
     * @return Exit code (0 on clean exit).
     */
    int handleTui() const;

    [[nodiscard]] manifests::ChaosManifest parseManifest(const std::string& path) const;

    [[nodiscard]] std::vector<std::unique_ptr<perturbations::IPerturbation>>
    buildPerturbations(const manifests::ChaosManifest& manifest) const;

    [[nodiscard]] shared::TargetState runPerturbationsLoop(
        std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations,
        const std::string& targetId,
        std::chrono::seconds duration) const;

    [[nodiscard]] bool validateExpectations(
        const manifests::ChaosManifest& manifest,
        const shared::TargetState& finalState) const;

    /**
     * @brief Internal helper to dispatch the parsed command.
     * @param args The arguments to parse.
     * @return Exit code (0 on success, non-zero on error).
     */
    int dispatchCommand(const std::vector<std::string>& args) const;
};

}  // namespace chaos::orchestrator::interfaces::cli
