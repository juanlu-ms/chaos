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

RunOrchestrator::RunOrchestrator(const ChaosService& service) : service_(service) {}

RunResult RunOrchestrator::run(const manifests::ChaosManifest& manifest, SharedState& state, IRunObserver& observer,
                               std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations,
                               std::stop_token external_stop) {
    const auto wall_start = std::chrono::system_clock::now();
    const auto steady_start = std::chrono::steady_clock::now();
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
        perturbations::PerturbationEngine pert_engine;
        pert_engine.scheduleAllAsync(std::move(perturbations), duration, external_stop);

        SPDLOG_INFO("Entering chaos phase: injecting faults for {}s.", duration.count());
        observer.onPhaseChange("chaos");
        state.setPhase("chaos");

        auto chaos_end = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < chaos_end && !external_stop.stop_requested()) {
            std::this_thread::sleep_for(100ms);
        }

        pert_engine.cancel();
        pert_engine.waitForTeardown();
    }

    // ── Recovery phase ────────────────────────────────────────────
    observer.onPhaseChange("recovery");
    SPDLOG_INFO("Entering recovery phase");
    state.setPhase("recovery");

    if (duration.count() > 0) {
        std::this_thread::sleep_for(2s);
    }

    // ── Validation ────────────────────────────────────────────────
    auto final_state = state.latestState();
    auto failures = state.continuousFailures();
    SPDLOG_INFO("Running final validation");

    RunResult result = service_.finalize(manifest, final_state, failures);
    const auto steady_end = std::chrono::steady_clock::now();
    result.manifest_name = manifest.test_name;
    result.target_id = manifest.target.id;
    result.duration_s = std::chrono::duration<double>(steady_end - steady_start).count();
    result.started_at = formatUtcIso8601(wall_start);
    return result;
}

}  // namespace chaos::orchestrator::core
