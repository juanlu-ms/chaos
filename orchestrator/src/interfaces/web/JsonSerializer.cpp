/**
 * @file JsonSerializer.cpp
 * @brief Implementation of JSON serialization helpers.
 */

#include "interfaces/web/JsonSerializer.hpp"

#include <string>
#include <thread>
#include <vector>

#include "validation/ValidationResult.hpp"

namespace chaos::orchestrator::interfaces::web {

json stateToJson(const core::TargetState& state, const std::string& phase,
                 const std::vector<std::string>& continuous_failures) {
    json result;
    result["container_id"] = state.container_id;
    result["status"] = containers::toString(state.status);
    if (state.cpu_usage_percent.has_value()) {
        result["cpu_usage_percent"] = state.cpu_usage_percent.value();
    }
    if (state.memory_usage_mb.has_value()) {
        result["memory_usage_mb"] = state.memory_usage_mb.value();
    }
    if (state.container_ip.has_value()) {
        result["container_ip"] = state.container_ip.value();
    }
    result["recent_logs"] = state.recent_logs;
    if (state.network_rx_bps.has_value()) {
        result["network_rx_bps"] = state.network_rx_bps.value();
    }
    if (state.network_tx_bps.has_value()) {
        result["network_tx_bps"] = state.network_tx_bps.value();
    }
    if (state.network_latency_ms.has_value()) {
        result["network_latency_ms"] = state.network_latency_ms.value();
    }
    if (!phase.empty()) {
        result["phase"] = phase;
    }
    if (!continuous_failures.empty()) {
        result["continuous_failures"] = continuous_failures;
    }
    return result;
}

json limitsToJson(const containers::SystemInfo& info) {
    json result;
    result["cpu_cores"] = std::thread::hardware_concurrency();
    auto memory_total_mb = static_cast<uint64_t>(info.mem_total / (static_cast<int64_t>(1024 * 1024)));
    result["memory_total_mb"] = memory_total_mb;
    result["perturbation_limits"] = {
        {"cpu_cap", {{"min_percent", 1}, {"max_percent", 100}}},
        {"memory_cap", {{"min_mb", 1}, {"max_mb", memory_total_mb}}},
        {"network_delay", {{"min_ms", 0}, {"max_ms", 30000}}},
    };
    return result;
}

}  // namespace chaos::orchestrator::interfaces::web
