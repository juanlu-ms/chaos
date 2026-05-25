/**
 * @file ChaosRunner.cpp
 * @brief Implementation of ChaosRunner shared business logic.
 */

#include "core/ChaosRunner.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "manifests/ManifestParser.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::core {

ChaosRunner::ChaosRunner(std::shared_ptr<containers::IContainerEngine> engine) : engine_(std::move(engine)) {}

std::vector<std::unique_ptr<perturbations::IPerturbation>> ChaosRunner::buildPerturbations(
    const manifests::ChaosManifest& manifest) const {
    perturbations::PerturbationFactory factory;
    std::vector<std::unique_ptr<perturbations::IPerturbation>> instances;
    instances.reserve(manifest.perturbations.size());
    for (const auto& spec : manifest.perturbations) {
        instances.push_back(factory.create(engine_, manifest.target, spec));
    }
    return instances;
}

RunResult ChaosRunner::finalize(const manifests::ChaosManifest& manifest, const core::TargetState& finalState,
                                const std::vector<std::string>& continuousFailures) const {
    if (manifest.expectations.empty()) {
        return {true, {}};
    }

    auto results = validation::validate(finalState, manifest.expectations);

    bool passed = true;
    for (auto& result : results) {
        if (bool continuousFailed =
                std::ranges::find(continuousFailures, result.expectationType) != continuousFailures.end()) {
            result.passed = false;
            result.message = "Passed final validation but failed mid-run continuous check";
        }
        if (!result.passed) {
            passed = false;
        }
    }

    return {passed, std::move(results)};
}

manifests::ChaosManifest ChaosRunner::parseManifest(const std::string& path) const {
    return manifests::ManifestParser::parseFromFile(path);
}

}  // namespace chaos::orchestrator::core
