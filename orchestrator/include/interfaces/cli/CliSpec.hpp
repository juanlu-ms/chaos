/**
 * @file CliSpec.hpp
 * @brief Declarative description of the chaos command-line surface.
 *
 * This table is the single source of truth for three consumers: the help text
 * (renderUsage), the bash completion script (generateBashCompletion) and
 * command dispatch in CliParser. Adding a command means adding one entry here
 * and one case to the dispatch switch; omitting the latter is a compile error.
 *
 * Every table constant is `inline constexpr` so the arrays referenced by the
 * std::span members have static storage duration and a single address across
 * translation units.
 */

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace chaos::orchestrator::interfaces::cli {

/** @brief Executable name used in the help text and the completion script. */
inline constexpr std::string_view kProgramName = "chaos";
/** @brief One-line product description shown under the usage line. */
inline constexpr std::string_view kTagline = "Chaos - Resilience Tool for Docker Containers";

/**
 * @brief Kind of value an option or positional argument accepts.
 *
 * Drives both the metavariable shown in the help text and the shell action
 * emitted by the bash completion generator.
 */
enum class ArgKind : std::uint8_t {
    /** @brief Takes no value; nothing is completed after it. */
    None,
    /** @brief Value is one of FlagSpec::values or CommandSpec::positionalWords. */
    Choice,
    /** @brief Value is a container id, completed from the container engine. */
    ContainerId,
    /** @brief Value is a manifest path; completion offers *.json files and directories. */
    ManifestPath,
    /** @brief Value is an arbitrary filesystem path. */
    FilePath,
    /** @brief Free-form text with no completion candidates. */
    Text,
};

/**
 * @brief Identifier of a top-level command.
 *
 * CliParser::dispatchCommand switches over this enum with no default label, so
 * adding an enumerator without adding a dispatch branch fails the build.
 */
enum class CommandId : std::uint8_t { List, Stop, Kill, Run, History, Completion, Serve, Help };

/**
 * @brief Layer responsible for executing a command.
 */
enum class CommandOwner : std::uint8_t {
    /** @brief Executed by CliParser::dispatchCommand. */
    Parser,
    /** @brief Intercepted by main() before the parser runs. */
    EntryPoint,
};

/**
 * @brief A single option accepted by the CLI.
 */
struct FlagSpec {
    /** @brief Long form including the leading dashes, e.g. "--log-level". */
    std::string_view longName{};
    /** @brief Short form including the leading dash; empty when there is none. */
    std::string_view shortName{};
    /** @brief Kind of value the option consumes. */
    ArgKind argKind{ArgKind::None};
    /** @brief Metavariable shown in the help text, e.g. "LEVEL"; empty for ArgKind::None. */
    std::string_view metavar{};
    /** @brief One-line description shown in the help text. */
    std::string_view description{};
    /** @brief Accepted values for ArgKind::Choice; empty otherwise. */
    std::span<const std::string_view> values{};
};

/**
 * @brief A top-level command.
 */
struct CommandSpec {
    /** @brief Command word as typed, e.g. "run". */
    std::string_view name{};
    /** @brief Identifier used for dispatch. */
    CommandId commandId{CommandId::Help};
    /** @brief Layer that executes the command. */
    CommandOwner owner{CommandOwner::Parser};
    /** @brief Argument summary appended to the name in the help text, e.g. "<container_id>". */
    std::string_view usage{};
    /** @brief One-line description shown in the help text. */
    std::string_view description{};
    /** @brief Options accepted after the command word. */
    std::span<const FlagSpec> flags{};
    /** @brief Kind of the command's positional argument; None when it takes none. */
    ArgKind positionalKind{ArgKind::None};
    /**
     * @brief Literal completion candidates for the positional argument.
     *
     * Suggestions only, not an exhaustive validation set: `history` accepts any
     * run id in addition to the literal "clear".
     */
    std::span<const std::string_view> positionalWords{};
    /** @brief Whether the positional argument occupies only the first slot after the command word. */
    bool positionalFirstOnly{false};
    /** @brief Alternative spellings accepted for this command, e.g. "-h" for "help". */
    std::span<const std::string_view> aliases{};
    /**
     * @brief Whether to omit the command from the help text and completion candidates.
     *
     * Hidden commands still dispatch normally when typed in full; this only
     * suppresses them as suggestions. Used for plumbing such as `completion`.
     */
    bool hidden{false};
};

/** @brief Accepted values for the global --log-level option. */
inline constexpr std::array<std::string_view, 7> kLogLevels{"trace", "debug",    "info", "warn",
                                                            "error", "critical", "off"};
/** @brief Alternative spellings of the `help` command. */
inline constexpr std::array<std::string_view, 2> kHelpAliases{"--help", "-h"};
/** @brief Literal completion candidates for the `history` positional argument. */
inline constexpr std::array<std::string_view, 1> kHistoryWords{"clear"};
/** @brief Shells for which `chaos completion` can emit a script. */
inline constexpr std::array<std::string_view, 1> kCompletionShells{"bash"};

/** @brief Options accepted before the command word. */
inline constexpr std::array<FlagSpec, 4> kGlobalFlags{
    FlagSpec{.longName = "--verbose", .shortName = "-v", .description = "Enable debug logging"},
    FlagSpec{.longName = "--quiet", .shortName = "-q", .description = "Suppress non-warning logs"},
    FlagSpec{.longName = "--log-level",
             .argKind = ArgKind::Choice,
             .metavar = "LEVEL",
             .description = "Set log level",
             .values = kLogLevels},
    FlagSpec{.longName = "--no-color", .description = "Disable colored output"},
};

/** @brief Options accepted by the `run` command. */
inline constexpr std::array<FlagSpec, 2> kRunFlags{
    FlagSpec{.longName = "--json", .description = "Emit machine-readable JSON to stdout instead of the human report"},
    FlagSpec{.longName = "--output",
             .argKind = ArgKind::FilePath,
             .metavar = "PATH",
             .description = "Write the JSON report to PATH (use '-' for stdout)"},
};

/** @brief Options accepted by the `history` command. */
inline constexpr std::array<FlagSpec, 1> kHistoryFlags{
    FlagSpec{.longName = "--json", .description = "Emit the history as JSON"},
};

/** @brief Options accepted by the `serve` command. */
inline constexpr std::array<FlagSpec, 1> kServeFlags{
    FlagSpec{.longName = "--port",
             .argKind = ArgKind::Text,
             .metavar = "PORT",
             .description = "TCP port to listen on (default 8080)"},
};

/** @brief Every top-level command, in help-text order. */
inline constexpr std::array<CommandSpec, 8> kCommands{
    CommandSpec{.name = "list", .commandId = CommandId::List, .description = "List all containers"},
    CommandSpec{.name = "stop",
                .commandId = CommandId::Stop,
                .usage = "<container_id>",
                .description = "Stop a running container",
                .positionalKind = ArgKind::ContainerId,
                .positionalFirstOnly = true},
    CommandSpec{.name = "kill",
                .commandId = CommandId::Kill,
                .usage = "<container_id>",
                .description = "Kill a running container",
                .positionalKind = ArgKind::ContainerId,
                .positionalFirstOnly = true},
    CommandSpec{.name = "run",
                .commandId = CommandId::Run,
                .usage = "<manifest.json>",
                .description = "Execute a chaos manifest",
                .flags = kRunFlags,
                .positionalKind = ArgKind::ManifestPath},
    CommandSpec{.name = "history",
                .commandId = CommandId::History,
                .usage = "[id]",
                .description = "View run history; 'history clear' wipes it",
                .flags = kHistoryFlags,
                .positionalKind = ArgKind::Choice,
                .positionalWords = kHistoryWords,
                .positionalFirstOnly = true},
    CommandSpec{.name = "completion",
                .commandId = CommandId::Completion,
                .usage = "<shell>",
                .description = "Print a shell completion script",
                .positionalKind = ArgKind::Choice,
                .positionalWords = kCompletionShells,
                .positionalFirstOnly = true,
                .hidden = true},
    CommandSpec{.name = "serve",
                .commandId = CommandId::Serve,
                .owner = CommandOwner::EntryPoint,
                .description = "Launch the Web UI and HTTP API",
                .flags = kServeFlags},
    CommandSpec{
        .name = "help", .commandId = CommandId::Help, .description = "Show this help message", .aliases = kHelpAliases},
};

/**
 * @brief Look up a command by name or alias.
 * @param name Candidate command word.
 * @return Pointer into kCommands, or nullptr when the word is not a command.
 */
[[nodiscard]] constexpr const CommandSpec* findCommand(std::string_view name) noexcept {
    for (const auto& command : kCommands) {
        if (command.name == name) {
            return &command;
        }
        for (const auto& alias : command.aliases) {
            if (alias == name) {
                return &command;
            }
        }
    }
    return nullptr;
}

/**
 * @brief Whether a word can be embedded verbatim in a double-quoted bash string.
 *
 * The completion generator relies on this holding for every word in the tables,
 * which lets it interpolate them without runtime shell escaping.
 */
[[nodiscard]] constexpr bool isShellSafeWord(std::string_view word) noexcept {
    if (word.empty()) {
        return false;
    }
    for (const char chr : word) {
        const bool safe = (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9') ||
                          chr == '-' || chr == '_' || chr == '.';
        if (!safe) {
            return false;
        }
    }
    return true;
}

namespace spec_detail {

[[nodiscard]] consteval bool allFlagWordsShellSafe(std::span<const FlagSpec> flags) noexcept {
    for (const auto& flag : flags) {
        if (!isShellSafeWord(flag.longName)) {
            return false;
        }
        if (!flag.shortName.empty() && !isShellSafeWord(flag.shortName)) {
            return false;
        }
        for (const auto& value : flag.values) {
            if (!isShellSafeWord(value)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] consteval bool allWordsShellSafe() noexcept {
    if (!allFlagWordsShellSafe(kGlobalFlags)) {
        return false;
    }
    for (const auto& command : kCommands) {
        if (!isShellSafeWord(command.name)) {
            return false;
        }
        for (const auto& alias : command.aliases) {
            if (!isShellSafeWord(alias)) {
                return false;
            }
        }
        for (const auto& word : command.positionalWords) {
            if (!isShellSafeWord(word)) {
                return false;
            }
        }
        if (!allFlagWordsShellSafe(command.flags)) {
            return false;
        }
    }
    return true;
}

}  // namespace spec_detail

static_assert(spec_detail::allWordsShellSafe(),
              "A CLI spec word is unsafe to embed verbatim in the generated completion script");

}  // namespace chaos::orchestrator::interfaces::cli
