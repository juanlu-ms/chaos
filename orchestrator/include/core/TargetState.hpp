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
    /** @brief Unique identifier of the target container. */
    std::string container_id;
    /** @brief Current lifecycle status of the container. */
    containers::ContainerStatus status;
    /** @brief CPU usage as a percentage of total allocated CPU, if available. */
    std::optional<double> cpu_usage_percent;
    /** @brief Memory usage in megabytes, if available. */
    std::optional<double> memory_usage_mb;
    /** @brief Primary IPv4 address of the container, if available. */
    std::optional<std::string> container_ip;
    /** @brief Recent stdout/stderr log lines from the container. */
    std::vector<std::string> recent_logs;
    /** @brief Network receive rate in bytes per second, if available. */
    std::optional<double> network_rx_bps;
    /** @brief Network transmit rate in bytes per second, if available. */
    std::optional<double> network_tx_bps;
    /** @brief Network round-trip latency in milliseconds, if available. */
    std::optional<double> network_latency_ms;
};

}  // namespace chaos::orchestrator::core
