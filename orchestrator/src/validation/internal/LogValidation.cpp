#include "LogValidation.hpp"

#include <fmt/format.h>

namespace chaos::orchestrator::validation {

ValidationResult LogContainsValidation::validate(const core::TargetState& target_state,
                                                 const manifests::Expectation& expectation) const {
    const auto& substring = expectation.parameters.at("substring");
    for (const auto& log : target_state.recent_logs) {
        if (log.contains(substring)) {
            return {
                .passed = true,
                .expectation_type = expectation.type,
                .message = fmt::format("Logs contain '{}':\n{}", substring, log),
            };
        }
    }
    return {
        .passed = false,
        .expectation_type = expectation.type,
        .message = fmt::format("Logs do NOT contain '{}'", substring),
    };
}

ValidationResult LogNotContainsValidation::validate(const core::TargetState& target_state,
                                                    const manifests::Expectation& expectation) const {
    const auto& substring = expectation.parameters.at("substring");
    for (const auto& log : target_state.recent_logs) {
        if (log.contains(substring)) {
            return {
                .passed = false,
                .expectation_type = expectation.type,
                .message = fmt::format("Logs CONTAIN '{}' (expected absent):\n{}", substring, log),
            };
        }
    }
    return {
        .passed = true,
        .expectation_type = expectation.type,
        .message = fmt::format("Logs do not contain '{}'", substring),
    };
}

}  // namespace chaos::orchestrator::validation
