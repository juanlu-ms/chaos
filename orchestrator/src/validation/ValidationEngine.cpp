/// @file ValidationEngine.cpp
/// @brief Evaluates manifest expectations against observed container state.

#include "validation/ValidationEngine.hpp"

#include <spdlog/spdlog.h>

#include <string>
#include <vector>

#include "validation/ValidationFactory.hpp"

namespace chaos::orchestrator::validation {

ValidationEngine::ValidationEngine(observability::ObservabilityEngine obs) : obs_(std::move(obs)) {}

std::vector<ValidationResult> ValidationEngine::validate(
    const std::string& containerId, const std::vector<manifests::Expectation>& expectations) const {
    std::vector<ValidationResult> results;
    results.reserve(expectations.size());

    // Cache logs lazily - only fetch if a log expectation is present.
    std::string cachedLogs;
    bool logsFetched = false;
    ValidationContext ctx{obs_, containerId, cachedLogs, logsFetched};

    ValidationFactory factory;
    for (const auto& expectation : expectations) {
        auto validator = factory.create(expectation);
        ValidationResult result = validator->validate(ctx, expectation);

        if (result.passed) {
            SPDLOG_INFO("[PASS] {}: {}", result.expectationType, result.message);
        } else {
            SPDLOG_ERROR("[FAIL] {}: {}", result.expectationType, result.message);
        }

        results.push_back(std::move(result));
    }

    return results;
}

}  // namespace chaos::orchestrator::validation
