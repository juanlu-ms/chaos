#include "validation/internal/HttpValidation.hpp"

#include <fmt/format.h>
#include <httplib.h>

#include <chrono>
#include <stdexcept>
#include <string>

namespace chaos::orchestrator::observability {
namespace validation_internal_detail {

std::string getStringParamOrDefault(const manifests::Expectation& expectation, const std::string& key,
                                    const std::string& defaultValue) {
    return expectation.parameters.contains(key) ? expectation.parameters.at(key) : defaultValue;
}

int getIntParamOrDefault(const manifests::Expectation& expectation, const std::string& key, int defaultValue) {
    return expectation.parameters.contains(key) ? std::stoi(expectation.parameters.at(key)) : defaultValue;
}

}  // namespace validation_internal_detail

ValidationResult HttpStatusValidation::validate(const ValidationContext& ctx,
                                                const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;

    try {
        const std::string ip = ctx.obs.getContainerIp(ctx.containerId);
        const std::string port = validation_internal_detail::getStringParamOrDefault(expectation, "port", "8000");
        const std::string path = validation_internal_detail::getStringParamOrDefault(expectation, "path", "/ping");
        const int expectedStatus =
            validation_internal_detail::getIntParamOrDefault(expectation, "expected_status", 200);

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

ValidationResult HttpLatencyValidation::validate(const ValidationContext& ctx,
                                                 const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;

    try {
        const std::string ip = ctx.obs.getContainerIp(ctx.containerId);
        const std::string port = validation_internal_detail::getStringParamOrDefault(expectation, "port", "8000");
        const std::string path = validation_internal_detail::getStringParamOrDefault(expectation, "path", "/ping");
        const int maxLatencyMs = validation_internal_detail::getIntParamOrDefault(expectation, "max_latency_ms", 1000);
        const int minLatencyMs = validation_internal_detail::getIntParamOrDefault(expectation, "min_latency_ms", 0);

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

}  // namespace chaos::orchestrator::observability
