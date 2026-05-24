#include "HttpValidation.hpp"

#include <fmt/format.h>
#include <httplib.h>

#include <charconv>
#include <chrono>
#include <stdexcept>
#include <string>

#include "containers/IContainerEngine.hpp"
#include "validation/internal/HttpValidatorBase.hpp"

namespace chaos::orchestrator::validation {

namespace {

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

}  // namespace

namespace detail {

ValidationResult performHttpValidation(const shared::TargetState& targetState,
                                       const manifests::Expectation& expectation, CheckFn checkFn) {
    ValidationResult result{.passed = false, .expectationType = expectation.type, .message = {}};

    try {
        const std::string ip =
            targetState.container_ip.value_or("Container IP is required for HTTP validation but was not available");
        const std::string port = getStringParamOrDefault(expectation, "port", "8000");
        const std::string path = getStringParamOrDefault(expectation, "path", "/ping");

        int portInt{};
        auto [ptr, ec] = std::from_chars(port.data(), port.data() + port.size(), portInt);
        if (ec != std::errc{}) {
            throw std::invalid_argument("Invalid port: " + port);
        }

        httplib::Client cli(ip, portInt);
        cli.set_connection_timeout(2);
        cli.set_read_timeout(5);

        const auto start = std::chrono::high_resolution_clock::now();
        const auto res = cli.Get(path);
        const auto end = std::chrono::high_resolution_clock::now();
        const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (res) {
            checkFn(res->status, static_cast<double>(durationMs), result);
            return result;
        }

        result.passed = false;
        result.message = fmt::format("HTTP GET {}:{} {} failed: {}", ip, port, path, httplib::to_string(res.error()));
    } catch (const std::invalid_argument& e) {
        result.passed = false;
        result.message = fmt::format("HTTP validation failed (invalid parameter): {}", e.what());
    } catch (const std::out_of_range& e) {
        result.passed = false;
        result.message = fmt::format("HTTP validation failed (out-of-range parameter): {}", e.what());
    } catch (const containers::ContainerEngineError& e) {
        result.passed = false;
        result.message = fmt::format("HTTP validation failed (container access): {}", e.what());
    }

    return result;
}

}  // namespace detail

ValidationResult HttpStatusValidation::validate(const shared::TargetState& targetState,
                                                const manifests::Expectation& expectation) const {
    try {
        return detail::performHttpValidation(
            targetState, expectation,
            [expected = getIntParamOrDefault(expectation, "expected_status", 200)](int statusCode, double,
                                                                                   ValidationResult& result) {
                result.passed = (statusCode == expected);
                result.message = result.passed
                                     ? fmt::format("HTTP GET returned status {}", statusCode)
                                     : fmt::format("HTTP GET returned status {} (expected {})", statusCode, expected);
            });
    } catch (const std::invalid_argument& e) {
        return ValidationResult{
            .passed = false,
            .expectationType = expectation.type,
            .message = fmt::format("HTTP Status validation failed (invalid parameter): {}", e.what())};
    } catch (const std::out_of_range& e) {
        return ValidationResult{.passed = false,
                                .expectationType = expectation.type,
                                .message = fmt::format("HTTP Status validation failed (out-of-range): {}", e.what())};
    }
}

ValidationResult HttpLatencyValidation::validate(const shared::TargetState& targetState,
                                                 const manifests::Expectation& expectation) const {
    try {
        return detail::performHttpValidation(
            targetState, expectation,
            [maxMs = getIntParamOrDefault(expectation, "max_latency_ms", 1000),
             minMs = getIntParamOrDefault(expectation, "min_latency_ms", 0)](int, double elapsedMs,
                                                                             ValidationResult& result) {
                result.passed = (elapsedMs >= minMs && elapsedMs <= maxMs);
                result.message =
                    result.passed ? fmt::format("HTTP Latency {:.0f}ms (expected {} - {}ms)", elapsedMs, minMs, maxMs)
                                  : fmt::format("HTTP Latency {:.0f}ms outside expected bounds ({} - {}ms)", elapsedMs,
                                                minMs, maxMs);
            });
    } catch (const std::invalid_argument& e) {
        return ValidationResult{
            .passed = false,
            .expectationType = expectation.type,
            .message = fmt::format("HTTP Latency validation failed (invalid parameter): {}", e.what())};
    } catch (const std::out_of_range& e) {
        return ValidationResult{.passed = false,
                                .expectationType = expectation.type,
                                .message = fmt::format("HTTP Latency validation failed (out-of-range): {}", e.what())};
    }
}

}  // namespace chaos::orchestrator::validation
