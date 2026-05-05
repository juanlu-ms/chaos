/// @file ValidationEngine.cpp
/// @brief Evaluates manifest expectations against observed container state.

#include "validation/ValidationEngine.hpp"

#include <spdlog/spdlog.h>

#include <vector>

#include "validation/ValidationFactory.hpp"

namespace chaos::orchestrator::validation {

std::vector<ValidationResult> ValidationEngine::validate(const shared::TargetState& targetState,
                                                         const std::vector<manifests::Expectation>& expectations) {
    std::vector<ValidationResult> results;
    results.reserve(expectations.size());

    ValidationFactory factory;
    for (const auto& expectation : expectations) {
        auto validator = factory.create(expectation);
        ValidationResult result = validator->validate(targetState, expectation);

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
