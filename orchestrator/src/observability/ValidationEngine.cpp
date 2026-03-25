/// @file ValidationEngine.cpp
/// @brief Evaluates manifest expectations against observed container state.

#include "observability/ValidationEngine.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace chaos::orchestrator::observability {

ValidationEngine::ValidationEngine(ObservabilityEngine obs) : obs_(std::move(obs)) {}

std::vector<ValidationResult> ValidationEngine::validate(
    const std::string& containerId, const std::vector<manifests::Expectation>& expectations) const {
    std::vector<ValidationResult> results;
    results.reserve(expectations.size());

    // Cache logs lazily — only fetch if a log expectation is present
    std::string cachedLogs;
    bool logsFetched = false;

    auto fetchLogs = [&]() -> const std::string& {
        if (!logsFetched) {
            cachedLogs = obs_.getLogs(containerId);
            logsFetched = true;
        }
        return cachedLogs;
    };

    for (const auto& expectation : expectations) {
        ValidationResult result;
        result.expectationType = expectation.type;

        if (expectation.type == "container_running") {
            result.passed = obs_.isRunning(containerId);
            result.message = result.passed ? "Container is running"
                                           : "Container is NOT running (expected running)";

        } else if (expectation.type == "container_not_running") {
            result.passed = !obs_.isRunning(containerId);
            result.message = result.passed ? "Container is not running"
                                           : "Container IS running (expected not running)";

        } else if (expectation.type == "log_contains") {
            const auto& substring = expectation.parameters.at("substring");
            const auto& logs = fetchLogs();
            result.passed = logs.find(substring) != std::string::npos;
            result.message = result.passed
                                 ? fmt::format("Logs contain '{}'", substring)
                                 : fmt::format("Logs do NOT contain '{}'", substring);

        } else if (expectation.type == "log_not_contains") {
            const auto& substring = expectation.parameters.at("substring");
            const auto& logs = fetchLogs();
            result.passed = logs.find(substring) == std::string::npos;
            result.message = result.passed
                                 ? fmt::format("Logs do not contain '{}'", substring)
                                 : fmt::format("Logs CONTAIN '{}' (expected absent)", substring);

        } else {
            result.passed = false;
            result.message = fmt::format("Unknown expectation type: '{}'", expectation.type);
        }

        if (result.passed) {
            SPDLOG_INFO("[PASS] {}: {}", result.expectationType, result.message);
        } else {
            SPDLOG_ERROR("[FAIL] {}: {}", result.expectationType, result.message);
        }

        results.push_back(std::move(result));
    }

    return results;
}

}  // namespace chaos::orchestrator::observability
