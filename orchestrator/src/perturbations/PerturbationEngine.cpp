/**
 * @file PerturbationEngine.cpp
 * @brief Implementation for the engine that manages perturbations.
 */

#include "perturbations/PerturbationEngine.hpp"

#include <spdlog/spdlog.h>

#include <ranges>

#include "perturbations/PerturbationFactory.hpp"

namespace chaos::orchestrator::perturbations {

PerturbationEngine::PerturbationEngine(std::shared_ptr<containers::IContainerEngine> engine)
    : engine_(std::move(engine)) {}

PerturbationEngine::~PerturbationEngine() { revertAll(); }

void PerturbationEngine::applyAll(const manifests::ChaosManifest& manifest) {
    PerturbationFactory factory;

    for (const auto& pert_spec : manifest.perturbations) {
        SPDLOG_INFO("Applying perturbation '{}'", pert_spec.type);
        auto perturbation = factory.create(engine_, manifest.target, pert_spec);
        perturbation->apply();
        // Only track successfully applied perturbations
        active_perturbations_.push_back(std::move(perturbation));
    }

    SPDLOG_INFO("All perturbations applied successfully.");
}

void PerturbationEngine::revertAll() {
    // Note: Reverse iteration because undoing state implies LIFO teardown
    for (auto const& active_perturbation : std::ranges::reverse_view(active_perturbations_)) {
        try {
            active_perturbation->revert();
        } catch (const std::exception& e) {
            // Note: During teardown, log exceptions but continue destroying context where possible.
            SPDLOG_ERROR("Failed to revert perturbation: {}", e.what());
        }
    }

    // Clear list tracking since everything that can be, has been reverted.
    active_perturbations_.clear();
}

}  // namespace chaos::orchestrator::perturbations
