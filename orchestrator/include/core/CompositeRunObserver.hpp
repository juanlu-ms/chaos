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

    void onStateUpdate(const core::TargetState& state) override;
    void onPhaseChange(std::string_view phase) override;
    void onLogsUpdate(const std::vector<std::string>& logs) override;
    void onNetworkLatencyUpdate(std::optional<double> latency) override;

private:
    std::vector<IRunObserver*> observers_;
};

}  // namespace chaos::orchestrator::core
