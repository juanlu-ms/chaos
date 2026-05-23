#include "core/RunOrchestrator.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>

#include "perturbations/PerturbationEngine.hpp"

namespace chaos::orchestrator::core {

using namespace std::chrono_literals;

RunOrchestrator::RunOrchestrator(ChaosRunner& runner) : runner_(runner) {}

RunResult RunOrchestrator::run(const manifests::ChaosManifest& manifest, SharedState& state, IRunObserver& observer,
                               std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations,
                               std::stop_token external_stop) {
    const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));

    // ── Normal phase ──────────────────────────────────────────────
    observer.onPhaseChange("normal");
    state.setPhase("normal");

    if (duration.count() > 0) {
        std::this_thread::sleep_for(2s);
    }

    // ── Chaos phase ───────────────────────────────────────────────
    if (duration.count() > 0 && !external_stop.stop_requested()) {
        perturbations::PerturbationEngine pertEngine;
        pertEngine.scheduleAllAsync(std::move(perturbations), duration, external_stop);

        SPDLOG_INFO("Injecting faults for {}s.", duration.count());
        observer.onPhaseChange("chaos");
        state.setPhase("chaos");

        auto chaosEnd = std::chrono::steady_clock::now() + duration + 2s;
        while (std::chrono::steady_clock::now() < chaosEnd && !external_stop.stop_requested()) {
            std::this_thread::sleep_for(100ms);
        }

        pertEngine.cancel();
        pertEngine.waitForTeardown();
    }

    // ── Recovery phase ────────────────────────────────────────────
    observer.onPhaseChange("recovery");
    state.setPhase("recovery");

    // ── Validation ────────────────────────────────────────────────
    auto finalState = state.latestState();
    auto failures = state.continuousFailures();
    return runner_.finalize(manifest, finalState, failures);
}

}  // namespace chaos::orchestrator::core
