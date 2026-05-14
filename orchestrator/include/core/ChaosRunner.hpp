/**
 * @file ChaosRunner.hpp
 * @brief Orchestrates chaos test lifecycle: manifest parsing, perturbation
 * construction, and expectation validation.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"
#include "shared/TargetState.hpp"
#include "validation/ValidationResult.hpp"

namespace chaos::orchestrator::containers {
class IContainerEngine;
}

namespace chaos::orchestrator::core {

/**
 * @brief Shared business logic for running chaos tests.
 *
 * Used by both the CLI and Server adapters to avoid duplicating
 * manifest parsing, perturbation construction, and validation logic.
 */
class ChaosRunner {
public:
    explicit ChaosRunner(std::shared_ptr<containers::IContainerEngine> engine);
    ~ChaosRunner() = default;

    ChaosRunner(const ChaosRunner&) = delete;
    ChaosRunner& operator=(const ChaosRunner&) = delete;

    /**
     * @brief Build perturbation instances from a manifest.
     * @throws std::invalid_argument If a perturbation type is unknown.
     */
    [[nodiscard]] std::vector<std::unique_ptr<perturbations::IPerturbation>> buildPerturbations(
        const manifests::ChaosManifest& manifest) const;

    /**
     * @brief Validate expectations against a final container state.
     * @return True if all expectations pass (or none are defined).
     */
    [[nodiscard]] bool validateExpectations(const manifests::ChaosManifest& manifest,
                                            const shared::TargetState& finalState) const;

    /**
     * @brief Parse a manifest from a JSON file.
     * @throws manifests::ManifestParserError If the file cannot be parsed.
     */
    [[nodiscard]] manifests::ChaosManifest parseManifest(const std::string& path) const;

private:
    std::shared_ptr<containers::IContainerEngine> engine_;
};

}  // namespace chaos::orchestrator::core
