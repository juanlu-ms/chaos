#pragma once

#include <vector>

#include "core/IRunObserver.hpp"

namespace chaos::orchestrator::core {

/**
 * @brief Multiplexer that forwards observer events to multiple IRunObserver implementations.
 *
 * Exception isolation: wraps each forward in try/catch; logs failures at warn level
 * via spdlog. Does not rethrow. Holds non-owning IRunObserver* pointers; callers
 * must ensure lifetimes outlive the composite.
 */
class CompositeRunObserver : public IRunObserver {
public:
    /**
     * @brief Construct with a list of non-owning observer pointers.
     * @param observers The observers to forward events to.
     */
    explicit CompositeRunObserver(std::vector<IRunObserver*> observers);

    /**
     * @brief Forward a state update event to all observers.
     * @param state The new target state.
     */
    void onStateUpdate(const core::TargetState& state) override;

    /**
     * @brief Forward a phase change event to all observers.
     * @param phase The new phase name.
     */
    void onPhaseChange(std::string_view phase) override;

    /**
     * @brief Forward a logs update event to all observers.
     * @param logs The new log lines.
     */
    void onLogsUpdate(const std::vector<std::string>& logs) override;

    /**
     * @brief Forward a network latency update to all observers.
     * @param latency The measured latency, or std::nullopt if unavailable.
     */
    void onNetworkLatencyUpdate(std::optional<double> latency) override;

private:
    std::vector<IRunObserver*> observers_;
};

}  // namespace chaos::orchestrator::core
