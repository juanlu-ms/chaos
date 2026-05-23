/**
 * @file RunOrchestrator.hpp
 * @brief Drives the chaos run lifecycle: normal → chaos → recovery → validate.
 */

#pragma once

#include <chrono>
#include <memory>
#include <stop_token>
#include <vector>

#include "core/ChaosRunner.hpp"
#include "core/IRunObserver.hpp"
#include "core/SharedState.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::core {

/**
 * @brief Drives the chaos run lifecycle: normal → chaos → recovery → validate.
 *
 * Blocks the calling thread for the full duration. Reads the latest state from
 * SharedState at finalization. Creates a local PerturbationEngine for the chaos
 * phase. Calls IRunObserver for phase transitions and ChaosRunner::finalize()
 * for validation.
 *
 * Does NOT observe — observation is handled by ObservationLoop running
 * concurrently in background threads.
 */
class RunOrchestrator {
public:
    /**
     * @brief Construct a RunOrchestrator.
     * @param runner Reference to the ChaosRunner used for validation.
     */
    explicit RunOrchestrator(ChaosRunner& runner);

    /**
     * @brief Execute a complete run lifecycle.
     * @param manifest The chaos test manifest.
     * @param state SharedState concurrently populated by ObservationLoop.
     * @param observer For phase/state notifications.
     * @param perturbations Built perturbation instances (moved in).
     * @param external_stop Token for cancellation (SIGINT, user abort, etc.).
     * @return RunResult with pass/fail and per-expectation details.
     */
    [[nodiscard]] RunResult run(const manifests::ChaosManifest& manifest, SharedState& state, IRunObserver& observer,
                                std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations,
                                std::stop_token external_stop = {});

private:
    ChaosRunner& runner_;
};

}  // namespace chaos::orchestrator::core
