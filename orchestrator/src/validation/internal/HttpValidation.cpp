#include "HttpValidation.hpp"

#include <fmt/format.h>
#include <httplib.h>

#include <charconv>
#include <chrono>
#include <stdexcept>
#include <string>

#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::validation {
namespace validation_internal_detail {

std::string getStringParamOrDefault(const manifests::Expectation& expectation, const std::string& key,
                                    const std::string& defaultValue) {
    return expectation.parameters.contains(key) ? expectation.parameters.at(key) : defaultValue;
}

int getIntParamOrDefault(const manifests::Expectation& expectation, const std::string& key, int defaultValue) {
    if (!expectation.parameters.contains(key)) {
        return defaultValue;
    }
    int val{};
    const auto& str = expectation.parameters.at(key);
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec != std::errc{}) {
        throw std::invalid_argument("Invalid integer value for '" + key + "': " + str);
    }
    return val;
}

}  // namespace validation_internal_detail

ValidationResult HttpStatusValidation::validate(const shared::TargetState& targetState,
                                                const manifests::Expectation& expectation) const {
    ValidationResult result{.passed = false, .expectationType = expectation.type, .message = {}};

    try {
        const std::string ip =
            targetState.container_ip.value_or("Container IP is required for HTTP validation but was not available");
        const std::string port = validation_internal_detail::getStringParamOrDefault(expectation, "port", "8000");
        const std::string path = validation_internal_detail::getStringParamOrDefault(expectation, "path", "/ping");
        const int expectedStatus =
            validation_internal_detail::getIntParamOrDefault(expectation, "expected_status", 200);
        const int portInt = [&] {
            int p{};
            auto [ptr, ec] = std::from_chars(port.data(), port.data() + port.size(), p);
            if (ec != std::errc{}) {
                throw std::invalid_argument("Invalid port: " + port);
            }
            return p;
        }();

        httplib::Client cli(ip, portInt);
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

ValidationResult HttpLatencyValidation::validate(const shared::TargetState& targetState,
                                                 const manifests::Expectation& expectation) const {
    ValidationResult result{.passed = false, .expectationType = expectation.type, .message = {}};

    try {
        const std::string ip =
            targetState.container_ip.value_or("Container IP is required for HTTP validation but was not available");
        const std::string port = validation_internal_detail::getStringParamOrDefault(expectation, "port", "8000");
        const std::string path = validation_internal_detail::getStringParamOrDefault(expectation, "path", "/ping");
        const int maxLatencyMs = validation_internal_detail::getIntParamOrDefault(expectation, "max_latency_ms", 1000);
        const int minLatencyMs = validation_internal_detail::getIntParamOrDefault(expectation, "min_latency_ms", 0);
        const int portInt = [&] {
            int p{};
            auto [ptr, ec] = std::from_chars(port.data(), port.data() + port.size(), p);
            if (ec != std::errc{}) throw std::invalid_argument("Invalid port: " + port);
            return p;
        }();

        httplib::Client cli(ip, portInt);
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

}  // namespace chaos::orchestrator::validation
