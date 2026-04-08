/// @file ValidationEngine.cpp
/// @brief Evaluates manifest expectations against observed container state.

#include "observability/ValidationEngine.hpp"

#include <fmt/format.h>
#include <httplib.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace chaos::orchestrator::observability {

// TODO: Extract expectation validators into separate files

namespace {

struct ValidationContext {
    const ObservabilityEngine& obs;
    const std::string& containerId;
    std::string& cachedLogs;
    bool& logsFetched;
};

const std::string& fetchLogs(const ValidationContext& ctx) {
    if (!ctx.logsFetched) {
        ctx.cachedLogs = ctx.obs.getLogs(ctx.containerId);
        ctx.logsFetched = true;
    }
    return ctx.cachedLogs;
}

std::string getStringParamOrDefault(const manifests::Expectation& expectation, const std::string& key,
                                    const std::string& defaultValue) {
    return expectation.parameters.contains(key) ? expectation.parameters.at(key) : defaultValue;
}

int getIntParamOrDefault(const manifests::Expectation& expectation, const std::string& key, int defaultValue) {
    return expectation.parameters.contains(key) ? std::stoi(expectation.parameters.at(key)) : defaultValue;
}

ValidationResult validateContainerRunning(const ValidationContext& ctx, const manifests::Expectation& expectation) {
    ValidationResult result;
    result.expectationType = expectation.type;
    result.passed = ctx.obs.isRunning(ctx.containerId);
    result.message = result.passed ? "Container is running" : "Container is NOT running (expected running)";
    return result;
}

ValidationResult validateContainerNotRunning(const ValidationContext& ctx, const manifests::Expectation& expectation) {
    ValidationResult result;
    result.expectationType = expectation.type;
    result.passed = !ctx.obs.isRunning(ctx.containerId);
    result.message = result.passed ? "Container is not running" : "Container IS running (expected not running)";
    return result;
}

ValidationResult validateLogContains(const ValidationContext& ctx, const manifests::Expectation& expectation) {
    ValidationResult result;
    result.expectationType = expectation.type;
    const auto& substring = expectation.parameters.at("substring");
    const auto& logs = fetchLogs(ctx);
    result.passed = logs.contains(substring);
    result.message = result.passed ? fmt::format("Logs contain '{}'", substring)
                                   : fmt::format("Logs do NOT contain '{}'", substring);
    return result;
}

ValidationResult validateLogNotContains(const ValidationContext& ctx, const manifests::Expectation& expectation) {
    ValidationResult result;
    result.expectationType = expectation.type;
    const auto& substring = expectation.parameters.at("substring");
    const auto& logs = fetchLogs(ctx);
    result.passed = !logs.contains(substring);
    result.message = result.passed ? fmt::format("Logs do not contain '{}'", substring)
                                   : fmt::format("Logs CONTAIN '{}' (expected absent)", substring);
    return result;
}

ValidationResult validateHttpStatus(const ValidationContext& ctx, const manifests::Expectation& expectation) {
    ValidationResult result;
    result.expectationType = expectation.type;

    try {
        const std::string ip = ctx.obs.getContainerIp(ctx.containerId);
        const std::string port = getStringParamOrDefault(expectation, "port", "8000");
        const std::string path = getStringParamOrDefault(expectation, "path", "/ping");
        const int expectedStatus = getIntParamOrDefault(expectation, "expected_status", 200);

        httplib::Client cli(ip, std::stoi(port));
        cli.set_connection_timeout(2);
        cli.set_read_timeout(5);

        const auto res = cli.Get(path);
        if (res) {
            result.passed = (res->status == expectedStatus);
            result.message = result.passed ? fmt::format("HTTP GET {}:{} {} returned {}", ip, port, path, res->status)
                                           : fmt::format("HTTP GET {}:{} {} returned {} (expected {})", ip, port, path,
                                                         res->status, expectedStatus);
            return result;
        }

        result.passed = false;
        result.message =
            fmt::format("HTTP GET {}:{} {} failed string error: {}", ip, port, path, httplib::to_string(res.error()));
    } catch (const std::invalid_argument& e) {
        result.passed = false;
        result.message = fmt::format("HTTP Status validation failed (invalid parameter): {}", e.what());
    } catch (const std::out_of_range& e) {
        result.passed = false;
        result.message = fmt::format("HTTP Status validation failed (out-of-range parameter): {}", e.what());
    } catch (const containers::ContainerEngineError& e) {
        result.passed = false;
        result.message = fmt::format("HTTP Status validation failed (container access): {}", e.what());
    }

    return result;
}

ValidationResult validateHttpLatency(const ValidationContext& ctx, const manifests::Expectation& expectation) {
    ValidationResult result;
    result.expectationType = expectation.type;

    try {
        const std::string ip = ctx.obs.getContainerIp(ctx.containerId);
        const std::string port = getStringParamOrDefault(expectation, "port", "8000");
        const std::string path = getStringParamOrDefault(expectation, "path", "/ping");
        const int maxLatencyMs = getIntParamOrDefault(expectation, "max_latency_ms", 1000);
        const int minLatencyMs = getIntParamOrDefault(expectation, "min_latency_ms", 0);

        httplib::Client cli(ip, std::stoi(port));
        cli.set_connection_timeout(2);
        cli.set_read_timeout(5);

        const auto start = std::chrono::high_resolution_clock::now();
        const auto res = cli.Get(path);
        const auto end = std::chrono::high_resolution_clock::now();
        const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (res) {
            result.passed = (durationMs <= maxLatencyMs && durationMs >= minLatencyMs);
            result.message = result.passed ? fmt::format("HTTP Latency {}ms (expected {} - {}ms)", durationMs,
                                                         minLatencyMs, maxLatencyMs)
                                           : fmt::format("HTTP Latency {}ms outside expected bounds ({} - {}ms)",
                                                         durationMs, minLatencyMs, maxLatencyMs);
            return result;
        }

        result.passed = false;
        result.message =
            fmt::format("HTTP GET {}:{} {} failed string error: {}", ip, port, path, httplib::to_string(res.error()));
    } catch (const std::invalid_argument& e) {
        result.passed = false;
        result.message = fmt::format("HTTP Latency validation failed (invalid parameter): {}", e.what());
    } catch (const std::out_of_range& e) {
        result.passed = false;
        result.message = fmt::format("HTTP Latency validation failed (out-of-range parameter): {}", e.what());
    } catch (const containers::ContainerEngineError& e) {
        result.passed = false;
        result.message = fmt::format("HTTP Latency validation failed (container access): {}", e.what());
    }

    return result;
}

ValidationResult validateUnknownExpectation(const manifests::Expectation& expectation) {
    ValidationResult result;
    result.expectationType = expectation.type;
    result.passed = false;
    result.message = fmt::format("Unknown expectation type: '{}'", expectation.type);
    return result;
}

using ValidatorFn = ValidationResult (*)(const ValidationContext&, const manifests::Expectation&);

inline constexpr std::array<std::pair<std::string_view, ValidatorFn>, 6> kValidatorFactory = {{
    {"container_running", &validateContainerRunning},
    {"container_not_running", &validateContainerNotRunning},
    {"log_contains", &validateLogContains},
    {"log_not_contains", &validateLogNotContains},
    {"http_status", &validateHttpStatus},
    {"http_latency", &validateHttpLatency},
}};

ValidationResult validateExpectation(const ValidationContext& ctx, const manifests::Expectation& expectation) {
    const auto it =
        std::ranges::find_if(kValidatorFactory, [&expectation](const auto& e) { return e.first == expectation.type; });
    if (it == kValidatorFactory.end()) {
        return validateUnknownExpectation(expectation);
    }
    return it->second(ctx, expectation);
}

}  // namespace

ValidationEngine::ValidationEngine(ObservabilityEngine obs) : obs_(std::move(obs)) {}

std::vector<ValidationResult> ValidationEngine::validate(
    const std::string& containerId, const std::vector<manifests::Expectation>& expectations) const {
    std::vector<ValidationResult> results;
    results.reserve(expectations.size());

    // Cache logs lazily — only fetch if a log expectation is present.
    std::string cachedLogs;
    bool logsFetched = false;
    ValidationContext ctx{obs_, containerId, cachedLogs, logsFetched};

    for (const auto& expectation : expectations) {
        ValidationResult result = validateExpectation(ctx, expectation);

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
