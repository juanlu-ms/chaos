#include "core/RunOrchestrator.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <ctime>
#include <string>
#include <thread>

#include "perturbations/PerturbationEngine.hpp"

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

}  // namespace

RunOrchestrator::RunOrchestrator(const ChaosRunner& runner) : runner_(runner) {}

RunResult RunOrchestrator::run(const manifests::ChaosManifest& manifest, SharedState& state, IRunObserver& observer,
                               std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations,
                               std::stop_token external_stop) {
    const auto wallStart = std::chrono::system_clock::now();
    const auto steadyStart = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));

    // ── Normal phase ──────────────────────────────────────────────
    observer.onPhaseChange("normal");
    SPDLOG_INFO("Entering normal phase");
    state.setPhase("normal");

    if (duration.count() > 0) {
        std::this_thread::sleep_for(2s);
    }

    // ── Chaos phase ───────────────────────────────────────────────
    if (duration.count() > 0 && !external_stop.stop_requested()) {
        perturbations::PerturbationEngine pertEngine;
        pertEngine.scheduleAllAsync(std::move(perturbations), duration, external_stop);

        SPDLOG_INFO("Entering chaos phase: injecting faults for {}s.", duration.count());
        observer.onPhaseChange("chaos");
        state.setPhase("chaos");

        auto chaosEnd = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < chaosEnd && !external_stop.stop_requested()) {
            std::this_thread::sleep_for(100ms);
        }

        pertEngine.cancel();
        pertEngine.waitForTeardown();
    }

    // ── Recovery phase ────────────────────────────────────────────
    observer.onPhaseChange("recovery");
    SPDLOG_INFO("Entering recovery phase");
    state.setPhase("recovery");

    if (duration.count() > 0) {
        std::this_thread::sleep_for(2s);
    }

    // ── Validation ────────────────────────────────────────────────
    auto finalState = state.latestState();
    auto failures = state.continuousFailures();
    SPDLOG_INFO("Running final validation");

    RunResult result = runner_.finalize(manifest, finalState, failures);
    const auto steadyEnd = std::chrono::steady_clock::now();
    result.manifest_name = manifest.test_name;
    result.target_id = manifest.target.id;
    result.duration_s = std::chrono::duration<double>(steadyEnd - steadyStart).count();
    result.started_at = formatUtcIso8601(wallStart);
    return result;
}

}  // namespace chaos::orchestrator::core
