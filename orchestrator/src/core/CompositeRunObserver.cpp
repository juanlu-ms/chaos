#include "core/CompositeRunObserver.hpp"

#include <spdlog/spdlog.h>

namespace chaos::orchestrator::core {

CompositeRunObserver::CompositeRunObserver(std::vector<IRunObserver*> observers) : observers_(std::move(observers)) {}

void CompositeRunObserver::onStateUpdate(const core::TargetState& state) {
    for (auto* observer : observers_) {
        try {
            observer->onStateUpdate(state);
        } catch (const std::exception& e) {
            SPDLOG_WARN("CompositeRunObserver: observer threw: {}", e.what());
        }
    }
}

void CompositeRunObserver::onPhaseChange(std::string_view phase) {
    for (auto* observer : observers_) {
        try {
            observer->onPhaseChange(phase);
        } catch (const std::exception& e) {
            SPDLOG_WARN("CompositeRunObserver: observer threw: {}", e.what());
        }
    }
}

void CompositeRunObserver::onLogsUpdate(const std::vector<std::string>& logs) {
    for (auto* observer : observers_) {
        try {
            observer->onLogsUpdate(logs);
        } catch (const std::exception& e) {
            SPDLOG_WARN("CompositeRunObserver: observer threw: {}", e.what());
        }
    }
}

void CompositeRunObserver::onNetworkLatencyUpdate(std::optional<double> latency) {
    for (auto* observer : observers_) {
        try {
            observer->onNetworkLatencyUpdate(latency);
        } catch (const std::exception& e) {
            SPDLOG_WARN("CompositeRunObserver: observer threw: {}", e.what());
        }
    }
}

}  // namespace chaos::orchestrator::core
