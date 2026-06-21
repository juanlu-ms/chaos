/**
 * @file LoggerSetup.cpp
 * @brief Implementation of centralized spdlog configuration.
 */

#include "observability/LoggerSetup.hpp"

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>

namespace chaos::orchestrator::observability {

void initLogger(spdlog::level::level_enum level, bool colorize) {
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    sink->set_color_mode(colorize ? spdlog::color_mode::automatic : spdlog::color_mode::never);

    auto logger = std::make_shared<spdlog::logger>("chaos", sink);
    logger->set_level(level);
    logger->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    spdlog::set_default_logger(logger);
}

}  // namespace chaos::orchestrator::observability
