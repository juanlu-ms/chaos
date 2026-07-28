/**
 * @file ChaosService.cpp
 * @brief Implementation of ChaosService shared business logic.
 */

#include "core/ChaosService.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "manifests/ManifestParser.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::core {

ChaosService::ChaosService(std::shared_ptr<containers::IContainerEngine> engine) : engine_(std::move(engine)) {}

std::vector<std::unique_ptr<perturbations::IPerturbation>> ChaosService::buildPerturbations(
    const manifests::ChaosManifest& manifest) const {
    perturbations::PerturbationFactory factory;
    std::vector<std::unique_ptr<perturbations::IPerturbation>> instances;
    instances.reserve(manifest.perturbations.size());
    SPDLOG_DEBUG("Building {} perturbation(s)", manifest.perturbations.size());
    for (const auto& spec : manifest.perturbations) {
        instances.push_back(factory.create(engine_, manifest.target, spec));
    }
    return instances;
}

RunResult ChaosService::finalize(const manifests::ChaosManifest& manifest, const core::TargetState& final_state,
                                 const std::vector<std::string>& continuous_failures) const {
    SPDLOG_DEBUG("Finalizing run with {} expectation(s)", manifest.expectations.size());
    if (manifest.expectations.empty()) {
        RunResult result;
        result.passed = true;
        return result;
    }

    auto results = validation::validate(final_state, manifest.expectations);

    bool passed = true;
    for (auto& result : results) {
        // A continuous failure always fails the expectation, but it only *explains* it when the
        // final check passed. When the final check failed too, its diagnosis is the useful one —
        // overwriting it would tell the user the expectation passed at the end when it did not.
        if (bool continuous_failed =
                std::ranges::find(continuous_failures, result.expectation_type) != continuous_failures.end()) {
            if (result.passed) {
                result.passed = false;
                result.message = "Passed final validation but failed mid-run continuous check";
            } else {
                result.message += " (also failed mid-run continuous check)";
            }
        }
        if (!result.passed) {
            passed = false;
        }
    }

    RunResult result;
    result.passed = passed;
    result.results = std::move(results);
    return result;
}

manifests::ChaosManifest ChaosService::parseManifest(const std::string& path) const {
    SPDLOG_DEBUG("Parsing manifest from '{}'", path);
    return manifests::ManifestParser::parseFromFile(path);
}

}  // namespace chaos::orchestrator::core
