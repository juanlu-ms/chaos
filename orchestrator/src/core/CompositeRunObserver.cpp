#include "core/CompositeRunObserver.hpp"

#include <spdlog/spdlog.h>

#include <functional>

namespace chaos::orchestrator::core {

namespace {

template <typename Func>
void forwardToAll(std::vector<std::reference_wrapper<IRunObserver>>& observers, Func&& fn) {
    for (IRunObserver& observer : observers) {
        try {
            std::invoke(std::forward<Func>(fn), observer);
        } catch (const std::exception& e) {
            SPDLOG_WARN("CompositeRunObserver: observer threw: {}", e.what());
        }
    }
}

}  // namespace

CompositeRunObserver::CompositeRunObserver(std::span<IRunObserver* const> observers) {
    observers_.reserve(observers.size());
    for (auto* obs : observers) {
        observers_.emplace_back(*obs);
    }
}

void CompositeRunObserver::onStateUpdate(const core::TargetState& state) {
    forwardToAll(observers_, [&](IRunObserver& o) { o.onStateUpdate(state); });
}

void CompositeRunObserver::onPhaseChange(std::string_view phase) {
    forwardToAll(observers_, [&](IRunObserver& o) { o.onPhaseChange(phase); });
}

void CompositeRunObserver::onLogsUpdate(const std::vector<std::string>& logs) {
    forwardToAll(observers_, [&](IRunObserver& o) { o.onLogsUpdate(logs); });
}

void CompositeRunObserver::onNetworkLatencyUpdate(std::optional<double> latency) {
    forwardToAll(observers_, [&](IRunObserver& o) { o.onNetworkLatencyUpdate(latency); });
}

}  // namespace chaos::orchestrator::core
