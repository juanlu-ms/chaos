#include "LogValidation.hpp"

#include <fmt/format.h>

namespace chaos::orchestrator::validation {

ValidationResult LogContainsValidation::validate(const shared::TargetState& targetState,
                                                 const manifests::Expectation& expectation) const {
    const auto& substring = expectation.parameters.at("substring");
    for (const auto& log : targetState.recent_logs) {
        if (log.contains(substring)) {
            return {
                .passed = true,
                .expectationType = expectation.type,
                .message = fmt::format("Logs contain '{}':\n{}", substring, log),
            };
        }
    }
    return {
        .passed = false,
        .expectationType = expectation.type,
        .message = fmt::format("Logs do NOT contain '{}'", substring),
    };
}

ValidationResult LogNotContainsValidation::validate(const shared::TargetState& targetState,
                                                    const manifests::Expectation& expectation) const {
    const auto& substring = expectation.parameters.at("substring");
    for (const auto& log : targetState.recent_logs) {
        if (log.contains(substring)) {
            return {
                .passed = false,
                .expectationType = expectation.type,
                .message = fmt::format("Logs CONTAIN '{}' (expected absent):\n{}", substring, log),
            };
        }
    }
    return {
        .passed = true,
        .expectationType = expectation.type,
        .message = fmt::format("Logs do not contain '{}'", substring),
    };
}

}  // namespace chaos::orchestrator::validation
