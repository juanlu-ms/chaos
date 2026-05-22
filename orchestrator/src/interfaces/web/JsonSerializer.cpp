/**
 * @file JsonSerializer.cpp
 * @brief Implementation of JSON serialization helpers.
 */

#include "web/JsonSerializer.hpp"

#include <string>
#include <thread>
#include <vector>

namespace chaos::orchestrator::interfaces::web {

json stateToJson(const shared::TargetState& state, const std::string& phase) {
    json result;
    result["container_id"] = state.container_id;
    result["status"] = shared::toString(state.status);
    if (state.cpu_usage_percent) {
        result["cpu_usage_percent"] = *state.cpu_usage_percent;
    }
    if (state.memory_usage_mb) {
        result["memory_usage_mb"] = *state.memory_usage_mb;
    }
    if (state.container_ip) {
        result["container_ip"] = *state.container_ip;
    }
    result["recent_logs"] = state.recent_logs;
    if (state.network_rx_bps) {
        result["network_rx_bps"] = *state.network_rx_bps;
    }
    if (state.network_tx_bps) {
        result["network_tx_bps"] = *state.network_tx_bps;
    }
    if (!phase.empty()) {
        result["phase"] = phase;
    }
    return result;
}

json limitsToJson(const containers::SystemInfo& info) {
    json result;
    result["cpu_cores"] = std::thread::hardware_concurrency();
    auto memoryTotalMb = static_cast<uint64_t>(info.memTotal / (static_cast<int64_t>(1024 * 1024)));
    result["memory_total_mb"] = memoryTotalMb;
    result["perturbation_limits"] = {
        {"cpu_cap", {{"min_percent", 1}, {"max_percent", 100}}},
        {"memory_cap", {{"min_mb", 1}, {"max_mb", memoryTotalMb}}},
        {"network_delay", {{"min_ms", 0}, {"max_ms", 30000}}},
    };
    return result;
}

}  // namespace chaos::orchestrator::interfaces::web
