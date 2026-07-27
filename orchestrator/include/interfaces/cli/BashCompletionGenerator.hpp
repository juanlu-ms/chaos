/**
 * @file BashCompletionGenerator.hpp
 * @brief Generates the bash completion script for the chaos CLI.
 */

#pragma once

#include <string>

namespace chaos::orchestrator::interfaces::cli {

/**
 * @brief Generate a self-contained bash completion script from the CLI spec table.
 *
 * The script defines `_chaos_completion`, registers it with `complete -F`, and
 * falls back to raw COMP_WORDS handling when bash-completion's
 * `_init_completion` is unavailable. Word lists are derived from kCommands and
 * kGlobalFlags; the surrounding shell logic is fixed boilerplate.
 *
 * @return The script, newline terminated.
 */
[[nodiscard]] std::string generateBashCompletion();

}  // namespace chaos::orchestrator::interfaces::cli
