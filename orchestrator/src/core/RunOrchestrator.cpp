#include "core/RunOrchestrator.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <ctime>
#include <string>
#include <thread>
#include <vector>

#include "core/RunPhase.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "validation/ValidationResult.hpp"

namespace chaos::orchestrator::core {

using namespace std::chrono_literals;

namespace {

[[nodiscard]] std::string formatUtcIso8601(std::chrono::system_clock::time_point tp) {
    const auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm utc{};
    gmtime_r(&time, &utc);
    std::array<char, 32> buf{};
    std::strftime(buf.data(), buf.size(), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return std::string(buf.data());
}

void enterRecoveryPhase(SharedState& state, IRunObserver& observer) {
    state.setPhase(RunPhase::Recovery);
    SPDLOG_INFO("Entering recovery phase");
    observer.onPhaseChange(toString(RunPhase::Recovery));
}

// Slack added to the hold time handed to the perturbation engine. Each perturbation runs its
// own timer, started when its apply() returned, and reverts on its own once that timer expires.
// Handing it the exact duration makes that timer race the orchestrator's chaos window — and the
// perturbation usually wins, reverting the fault while the run is still labelled "chaos". The
// grace keeps the engine's timer strictly behind the orchestrator, so cancel() is what ends the
// fault and the engine's timer is only a backstop for an orchestrator that never gets there.
constexpr auto kPerturbationHoldGrace = std::chrono::seconds(2);

}  // namespace

RunOrchestrator::RunOrchestrator(const ChaosService& service) : service_(service) {}

RunResult RunOrchestrator::run(const manifests::ChaosManifest& manifest, SharedState& state, IRunObserver& observer,
                               std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations,
                               std::stop_token external_stop) {
    const auto wall_start = std::chrono::system_clock::now();
    const auto steady_start = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));

    // ── Normal phase ──────────────────────────────────────────────
    // State first, then observers: an observer woken by the notification must see the new phase.
    state.setPhase(RunPhase::Normal);
    SPDLOG_INFO("Entering normal phase");
    observer.onPhaseChange(toString(RunPhase::Normal));

    if (duration.count() > 0) {
        std::this_thread::sleep_for(2s);
    }

    // ── Chaos phase ───────────────────────────────────────────────
    std::vector<std::string> apply_failures;
    if (duration.count() > 0 && !external_stop.stop_requested()) {
        perturbations::PerturbationEngine pert_engine;
        pert_engine.scheduleAllAsync(std::move(perturbations), duration + kPerturbationHoldGrace, external_stop);

        SPDLOG_INFO("Entering chaos phase: injecting faults for {}s.", duration.count());
        state.setPhase(RunPhase::Chaos);
        observer.onPhaseChange(toString(RunPhase::Chaos));

        auto chaos_end = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < chaos_end && !external_stop.stop_requested()) {
            std::this_thread::sleep_for(100ms);
        }

        // ── Recovery phase ────────────────────────────────────────
        // Entered before the revert, not after: undoing a fault (restarting a killed container,
        // dropping tc rules) is not instantaneous, and continuous failures latch permanently.
        // Leaving the teardown window labelled "chaos" would fail the run for disruption the
        // fault itself never caused.
        enterRecoveryPhase(state, observer);

        pert_engine.cancel();
        pert_engine.waitForTeardown();
        apply_failures = pert_engine.applyFailures();
    } else {
        // ── Recovery phase ────────────────────────────────────────
        enterRecoveryPhase(state, observer);
    }

    if (duration.count() > 0) {
        std::this_thread::sleep_for(2s);
    }

    // ── Validation ────────────────────────────────────────────────
    auto final_state = state.latestState();
    auto failures = state.continuousFailures();
    SPDLOG_INFO("Running final validation");

    RunResult result = service_.finalize(manifest, final_state, failures);

    // A perturbation that failed to apply means the fault was never injected, so the run must
    // not be reported as passing regardless of what the expectations observed on a healthy target.
    if (!apply_failures.empty()) {
        result.passed = false;
        for (const auto& msg : apply_failures) {
            result.results.push_back(validation::ValidationResult{
                .passed = false, .expectation_type = "perturbation", .message = "Failed to inject fault: " + msg});
        }
    }

    const auto steady_end = std::chrono::steady_clock::now();
    result.manifest_name = manifest.test_name;
    result.target_id = manifest.target.id;
    result.duration_s = std::chrono::duration<double>(steady_end - steady_start).count();
    result.started_at = formatUtcIso8601(wall_start);
    return result;
}

}  // namespace chaos::orchestrator::core
