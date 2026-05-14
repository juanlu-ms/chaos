/**
 * @file ChaosRunner.cpp
 * @brief Implementation of ChaosRunner shared business logic.
 */

#include "core/ChaosRunner.hpp"

#include <spdlog/spdlog.h>

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

bool ChaosRunner::validateExpectations(const manifests::ChaosManifest& manifest,
                                       const shared::TargetState& finalState) const {
    if (manifest.expectations.empty()) {
        return true;
    }

    SPDLOG_INFO("Evaluating {} expectation(s)...", manifest.expectations.size());
    const auto results = validation::validate(finalState, manifest.expectations);

    for (const auto& result : results) {
        if (!result.passed) {
            SPDLOG_ERROR("Chaos run FAILED: one or more expectations were not met.");
            return false;
        }
    }
    SPDLOG_INFO("Chaos run PASSED: all expectations met.");
    return true;
}

manifests::ChaosManifest ChaosRunner::parseManifest(const std::string& path) const {
    return manifests::ManifestParser::parseFromFile(path);
}

}  // namespace chaos::orchestrator::core
