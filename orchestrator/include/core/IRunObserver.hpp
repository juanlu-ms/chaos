/**
 * @file IRunObserver.hpp
 * @brief Observer interface for receiving run lifecycle events.
 */

#pragma once

#include <string_view>
#include <vector>

#include "core/RunResult.hpp"
#include "core/TargetState.hpp"

namespace chaos::orchestrator::core {

/**
 * @brief Observer interface for run lifecycle events.
 *
 * Implementations translate these events into UI updates (CLI prints, SSE
 * push, TUI rendering, etc.) without coupling the run logic to the
 * delivery mechanism.
 */
class IRunObserver {
public:
    /**
     * @brief Called when the target state is updated.
     * @param state The newly observed state of the target.
     */
    virtual void onStateUpdate(const core::TargetState& state) = 0;

    /**
     * @brief Called when the current execution phase changes (e.g. normal, chaos, recovery).
     * @param phase The name of the new phase.
     */
    virtual void onPhaseChange(std::string_view phase) = 0;

    /**
     * @brief Called when new log lines are available.
     * @param logs The latest log lines from the target container.
     */
    virtual void onLogsUpdate(const std::vector<std::string>& /*logs*/) {}

    /**
     * @brief Called when network latency is measured.
     * @param latency RTT in milliseconds, or std::nullopt if unavailable.
     */
    virtual void onNetworkLatencyUpdate(std::optional<double> /*latency*/) {}

    virtual ~IRunObserver() = default;
};

}  // namespace chaos::orchestrator::core
