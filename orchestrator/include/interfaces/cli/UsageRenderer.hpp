/**
 * @file UsageRenderer.hpp
 * @brief Renders the chaos help text from the CLI spec table.
 */

#pragma once

#include <string>

namespace chaos::orchestrator::interfaces::cli {

/**
 * @brief Render the full help text from kGlobalFlags and kCommands.
 *
 * Sections are emitted in order: Global Options, Commands, then one
 * "<Command> Options:" section for each command that declares options. All
 * description columns share a single alignment width computed across sections.
 *
 * @return Help text, newline terminated.
 */
[[nodiscard]] std::string renderUsage();

}  // namespace chaos::orchestrator::interfaces::cli
