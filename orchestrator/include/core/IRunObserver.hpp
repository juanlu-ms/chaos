/**
 * @file IRunObserver.hpp
 * @brief Observer interface for receiving run lifecycle events.
 */

#pragma once

#include <string_view>

#include "shared/TargetState.hpp"

namespace chaos::orchestrator::core {

struct RunResult;

/**
 * @brief Observer interface for run lifecycle events.
 *
 * Implementations translate these events into UI updates (CLI prints, SSE
 * push, TUI rendering, etc.) without coupling the run logic to the
 * delivery mechanism.
 */
struct IRunObserver {
    /**
     * @brief Called when the target state is updated.
     * @param state The newly observed state of the target.
     */
    virtual void onStateUpdate(const shared::TargetState& state) = 0;

    /**
     * @brief Called when the current execution phase changes (e.g. normal, chaos, recovery).
     * @param phase The name of the new phase.
     */
    virtual void onPhaseChange(std::string_view phase) = 0;

    virtual ~IRunObserver() = default;
};

}  // namespace chaos::orchestrator::core
