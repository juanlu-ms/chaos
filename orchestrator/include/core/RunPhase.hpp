/**
 * @file RunPhase.hpp
 * @brief The phases a chaos run moves through, and their wire names.
 */

#pragma once

#include <string_view>

namespace chaos::orchestrator::core {

/**
 * @brief Phase of a chaos run.
 *
 * The internal source of truth for "where are we in the run". Converted to a
 * string only at boundaries that already speak strings (IRunObserver, the
 * persisted history records, the OTLP payload and the SSE stream) so those
 * formats stay unchanged.
 */
enum class RunPhase {
    /** @brief Before any fault is injected. */
    Normal,
    /** @brief Fault applied; the window the expectations are meant to judge. */
    Chaos,
    /** @brief Fault reverted; the target is given time to come back. */
    Recovery
};

/**
 * @brief Wire name of a phase, as used in history JSON, OTLP and SSE payloads.
 * @param phase The phase to name.
 * @return "normal", "chaos" or "recovery".
 */
[[nodiscard]] constexpr std::string_view toString(RunPhase phase) noexcept {
    switch (phase) {
        case RunPhase::Normal:
            return "normal";
        case RunPhase::Chaos:
            return "chaos";
        case RunPhase::Recovery:
            return "recovery";
    }
    return "normal";
}

}  // namespace chaos::orchestrator::core
