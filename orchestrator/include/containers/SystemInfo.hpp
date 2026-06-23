/**
 * @file SystemInfo.hpp
 * @brief System-level information from the container engine.
 */

#pragma once

#include <cstdint>

namespace chaos::orchestrator::containers {

/**
 * @brief System-level information from the container engine daemon.
 */
struct SystemInfo {
    /** @brief Total physical memory in bytes. */
    int64_t mem_total = 0;
};

}  // namespace chaos::orchestrator::containers
