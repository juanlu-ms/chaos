/**
 * @file ChaosService.hpp
 * @brief Orchestrates chaos test lifecycle: manifest parsing, perturbation
 * construction, and expectation validation.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/RunResult.hpp"
#include "core/TargetState.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"

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
class ChaosService {
public:
    /**
     * @brief Construct a ChaosService with a container engine backend.
     * @param engine Shared pointer to the container engine implementation.
     */
    explicit ChaosService(std::shared_ptr<containers::IContainerEngine> engine);
    ~ChaosService() = default;

    ChaosService(const ChaosService&) = delete;
    ChaosService& operator=(const ChaosService&) = delete;

    /**
     * @brief Build perturbation instances from a manifest.
     * @param manifest The chaos manifest containing perturbation definitions.
     * @return Vector of unique pointers to constructed perturbation objects.
     * @throws std::invalid_argument If a perturbation type is unknown.
     */
    [[nodiscard]] std::vector<std::unique_ptr<perturbations::IPerturbation>> buildPerturbations(
        const manifests::ChaosManifest& manifest) const;

    /**
     * @brief Validate expectations and return full results.
     * @param manifest The chaos test manifest with expectations.
     * @param final_state The final observed container state.
     * @param continuous_failures Optional list of expectation types that failed mid-run.
     * @return RunResult with pass/fail and per-expectation details.
     */
    [[nodiscard]] RunResult finalize(const manifests::ChaosManifest& manifest, const core::TargetState& final_state,
                                     const std::vector<std::string>& continuous_failures = {}) const;

    /**
     * @brief Parse a manifest from a JSON file.
     * @param path Filesystem path to the JSON manifest file.
     * @return The parsed ChaosManifest with perturbations and expectations.
     * @throws manifests::ManifestParserError If the file cannot be parsed.
     */
    [[nodiscard]] manifests::ChaosManifest parseManifest(const std::string& path) const;

private:
    std::shared_ptr<containers::IContainerEngine> engine_;
};

}  // namespace chaos::orchestrator::core
