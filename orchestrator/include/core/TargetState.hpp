/**
 * @file TargetState.hpp
 * @brief Definition of TargetState struct representing the observed state
 * of a target container.
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "containers/ContainerStatus.hpp"

namespace chaos::orchestrator::core {

/**
 * @brief Represents the observed state of a target at a given point in time.
 * This acts as a neutral Data Transfer Object (DTO) between Observability and Validation.
 */
struct TargetState {
    std::string container_id;
    containers::ContainerStatus status;
    std::optional<double> cpu_usage_percent;
    std::optional<double> memory_usage_mb;
    std::optional<std::string> container_ip;
    std::vector<std::string> recent_logs;
    std::optional<double> network_rx_bps;
    std::optional<double> network_tx_bps;
    std::optional<double> network_latency_ms;
};

}  // namespace chaos::orchestrator::core
