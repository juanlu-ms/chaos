#include "validation/LogValidation.hpp"

#include <fmt/format.h>

namespace chaos::orchestrator::validation {

ValidationResult LogContainsValidation::validate(const ValidationContext& ctx,
                                                 const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;
    const auto& substring = expectation.parameters.at("substring");
    const auto& logs = ctx.fetchLogs();
    result.passed = logs.contains(substring);
    result.message = result.passed ? fmt::format("Logs contain '{}'", substring)
                                   : fmt::format("Logs do NOT contain '{}'", substring);
    return result;
}

ValidationResult LogNotContainsValidation::validate(const ValidationContext& ctx,
                                                    const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;
    const auto& substring = expectation.parameters.at("substring");
    const auto& logs = ctx.fetchLogs();
    result.passed = !logs.contains(substring);
    result.message = result.passed ? fmt::format("Logs do not contain '{}'", substring)
                                   : fmt::format("Logs CONTAIN '{}' (expected absent)", substring);
    return result;
}

}  // namespace chaos::orchestrator::validation
