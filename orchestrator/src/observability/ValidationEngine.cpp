/// @file ValidationEngine.cpp
/// @brief Evaluates manifest expectations against observed container state.

#include "observability/ValidationEngine.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <string>
#include <vector>

#include <httplib.h>

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

        } else if (expectation.type == "http_status") {
            try {
                const std::string ip = obs_.getContainerIp(containerId);
                const std::string port = expectation.parameters.contains("port") ? expectation.parameters.at("port") : "8000";
                const std::string path = expectation.parameters.contains("path") ? expectation.parameters.at("path") : "/ping";
                const int expected_status = expectation.parameters.contains("expected_status") ? std::stoi(expectation.parameters.at("expected_status")) : 200;

                httplib::Client cli(ip, std::stoi(port));
                cli.set_connection_timeout(2);
                cli.set_read_timeout(5);

                const auto res = cli.Get(path);
                if (res) {
                    result.passed = (res->status == expected_status);
                    result.message = result.passed
                                         ? fmt::format("HTTP GET {}:{} {} returned {}", ip, port, path, res->status)
                                         : fmt::format("HTTP GET {}:{} {} returned {} (expected {})", ip, port, path, res->status, expected_status);
                } else {
                    result.passed = false;
                    result.message = fmt::format("HTTP GET {}:{} {} failed string error: {}", ip, port, path, httplib::to_string(res.error()));
                }
            } catch (const std::exception& e) {
                result.passed = false;
                result.message = fmt::format("HTTP Status validation failed: {}", e.what());
            }

        } else if (expectation.type == "http_latency") {
            try {
                const std::string ip = obs_.getContainerIp(containerId);
                const std::string port = expectation.parameters.contains("port") ? expectation.parameters.at("port") : "8000";
                const std::string path = expectation.parameters.contains("path") ? expectation.parameters.at("path") : "/ping";
                const int max_latency = expectation.parameters.contains("max_latency_ms") ? std::stoi(expectation.parameters.at("max_latency_ms")) : 1000;
                const int min_latency = expectation.parameters.contains("min_latency_ms") ? std::stoi(expectation.parameters.at("min_latency_ms")) : 0;

                httplib::Client cli(ip, std::stoi(port));
                cli.set_connection_timeout(2);
                cli.set_read_timeout(5);

                const auto start = std::chrono::high_resolution_clock::now();
                const auto res = cli.Get(path);
                const auto end = std::chrono::high_resolution_clock::now();
                const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

                if (res) {
                    result.passed = (duration_ms <= max_latency && duration_ms >= min_latency);
                    result.message = result.passed
                                         ? fmt::format("HTTP Latency {}ms (expected {} - {}ms)", duration_ms, min_latency, max_latency)
                                         : fmt::format("HTTP Latency {}ms outside expected bounds ({} - {}ms)", duration_ms, min_latency, max_latency);
                } else {
                    result.passed = false;
                    result.message = fmt::format("HTTP GET {}:{} {} failed string error: {}", ip, port, path, httplib::to_string(res.error()));
                }
            } catch (const std::exception& e) {
                result.passed = false;
                result.message = fmt::format("HTTP Latency validation failed: {}", e.what());
            }

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
