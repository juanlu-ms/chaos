#include "LogValidation.hpp"

#include <fmt/format.h>

namespace chaos::orchestrator::validation {

ValidationResult LogContainsValidation::validate(const shared::TargetState& targetState,
                                                 const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;
    const auto& substring = expectation.parameters.at("substring");
    for (const auto& log : targetState.recent_logs) {
        if (log.contains(substring)) {
            result.passed = true;
            result.message = fmt::format("Logs contain '{}':\n{}", substring, log);
            return result;
        }
    }
    result.passed = false;
    result.message = fmt::format("Logs do NOT contain '{}'", substring);
    return result;
}

ValidationResult LogNotContainsValidation::validate(const shared::TargetState& targetState,
                                                    const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;
    const auto& substring = expectation.parameters.at("substring");
    for (const auto& log : targetState.recent_logs) {
        if (log.contains(substring)) {
            result.passed = false;
            result.message = fmt::format("Logs CONTAIN '{}' (expected absent):\n{}", substring, log);
            return result;
        }
    }
    result.passed = true;
    result.message = fmt::format("Logs do not contain '{}'", substring);
    return result;
}

}  // namespace chaos::orchestrator::validation
