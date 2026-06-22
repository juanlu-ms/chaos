/**
 * @file CliParser.hpp
 * @brief Command-line interface adapter for the chaos orchestrator.
 */

#pragma once

#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "containers/IContainerEngine.hpp"
#include "core/ChaosRunner.hpp"
#include "history/IRunHistory.hpp"

namespace chaos::orchestrator::interfaces::cli {

/**
 * @brief Logging-related global flags parsed before any log line is emitted.
 */
struct GlobalOptions {
    /** @brief Minimum log level for the default spdlog logger. */
    spdlog::level::level_enum logLevel{spdlog::level::info};
    /** @brief Whether ANSI color output is enabled. */
    bool colorize{true};
};

/**
 * @brief Result of stripping global flags from the raw argv.
 */
struct ParsedGlobalFlags {
    /** @brief Parsed options. */
    GlobalOptions options;
    /** @brief Cleaned argv (program name still at index 0). */
    std::vector<char*> args;
    /** @brief Set if a global flag was malformed. */
    std::optional<std::string> error;
};

/**
 * @brief Parse global logging/color flags and return a cleaned argv.
 *
 * Recognized flags are removed from the returned args vector so that the
 * rest of the CLI parser only sees the program name and command arguments.
 *
 * @param argv Raw argument values from main(), including executable at index 0.
 * @return Parsed options and the cleaned argument list.
 */
[[nodiscard]] ParsedGlobalFlags parseGlobalFlags(std::span<char*> argv);

/**
 * @brief Options specific to the 'run' subcommand.
 */
struct RunOptions {
    /** @brief Emit JSON to stdout instead of the human report. */
    bool jsonOutput{false};
    /** @brief Write JSON report to this path; '-' means stdout. */
    std::optional<std::string> outputPath;
};

/**
 * @brief CLI adapter for the chaos orchestrator.
 */
class CliParser {
public:
    /**
     * @brief Construct the CLI adapter with a container engine.
     * @param engine Non-owning reference to the container engine port.
     */
    explicit CliParser(std::shared_ptr<containers::IContainerEngine> engine,
                       std::shared_ptr<history::IRunHistory> history = nullptr);
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
    std::shared_ptr<containers::IContainerEngine> engine_;
    std::shared_ptr<history::IRunHistory> history_;
    core::ChaosRunner runner_;

    void printUsage() const;
    int handleList() const;
    int handleStop(const std::string& containerId) const;
    int handleKill(const std::string& containerId) const;
    int handleRun(const std::string& manifestPath, const RunOptions& options) const;
    void printRunResults(const core::RunResult& results, const manifests::ChaosManifest& manifest,
                         std::chrono::duration<double> elapsed) const;
    int handleHistory(const std::vector<std::string>& args) const;
    int dispatchCommand(const std::vector<std::string>& args) const;
};

}  // namespace chaos::orchestrator::interfaces::cli
