/**
 * @file LoggerSetup.hpp
 * @brief Centralized spdlog configuration for the chaos orchestrator.
 */

#pragma once

#include <spdlog/spdlog.h>

namespace chaos::orchestrator::observability {

/**
 * @brief Configure the default spdlog logger once at program startup.
 *
 * Replaces spdlog's default pattern with a concise CLI-friendly format and
 * wires the requested log level / color mode. This should be called before
 * the first SPDLOG_* macro so that every module sees the same sink.
 *
 * @param level   Minimum log level emitted by the default logger.
 * @param colorize When false, ANSI color codes are disabled. When true,
 *                 color is applied automatically only when stdout is a TTY.
 */
void initLogger(spdlog::level::level_enum level, bool colorize);

}  // namespace chaos::orchestrator::observability
