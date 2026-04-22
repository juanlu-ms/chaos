/**
 * @file TargetState.hpp
 * @brief Definition of TargetState struct representing the observed state
 * of a target container.
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "shared/ContainerStatus.hpp"

namespace chaos::orchestrator::shared {

/**
 * @brief Represents the observed state of a target at a given point in time.
 * This acts as a neutral Data Transfer Object (DTO) between Observability and Validation.
 */
struct TargetState {
    std::string container_id;
    ContainerStatus status;
    std::optional<double> cpu_usage_percent;
    std::optional<double> memory_usage_mb;
    std::optional<std::string> container_ip;
    std::vector<std::string> recent_logs;
};

}  // namespace chaos::orchestrator::shared
