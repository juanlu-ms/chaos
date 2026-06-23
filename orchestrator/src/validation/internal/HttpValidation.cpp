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
                                    const std::string& default_value) {
    return expectation.parameters.contains(key) ? expectation.parameters.at(key) : default_value;
}

int getIntParamOrDefault(const manifests::Expectation& expectation, const std::string& key, int default_value) {
    if (!expectation.parameters.contains(key)) {
        return default_value;
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

ValidationResult performHttpValidation(const core::TargetState& target_state, const manifests::Expectation& expectation,
                                       CheckFn check_fn) {
    ValidationResult result{.passed = false, .expectation_type = expectation.type, .message = {}};

    try {
        if (!target_state.container_ip.has_value()) {
            result.message = "Container IP is required for HTTP validation but was not available";
            return result;
        }
        const std::string ip = target_state.container_ip.value();
        const std::string port = getStringParamOrDefault(expectation, "port", "8000");
        const std::string path = getStringParamOrDefault(expectation, "path", "/ping");

        int port_int{};
        auto [ptr, ec] = std::from_chars(port.data(), port.data() + port.size(), port_int);
        if (ec != std::errc{}) {
            throw std::invalid_argument("Invalid port: " + port);
        }

        httplib::Client cli(ip, port_int);
        cli.set_connection_timeout(2);
        cli.set_read_timeout(5);

        const auto start = std::chrono::high_resolution_clock::now();
        const auto res = cli.Get(path);
        const auto end = std::chrono::high_resolution_clock::now();
        const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (res) {
            check_fn(res->status, static_cast<double>(duration_ms), result);
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

ValidationResult HttpStatusValidation::validate(const core::TargetState& target_state,
                                                const manifests::Expectation& expectation) const {
    try {
        return detail::performHttpValidation(
            target_state, expectation,
            [expected = getIntParamOrDefault(expectation, "expected_status", 200)](int status_code, double,
                                                                                   ValidationResult& result) {
                result.passed = (status_code == expected);
                result.message = result.passed
                                     ? fmt::format("HTTP GET returned status {}", status_code)
                                     : fmt::format("HTTP GET returned status {} (expected {})", status_code, expected);
            });
    } catch (const std::invalid_argument& e) {
        return ValidationResult{
            .passed = false,
            .expectation_type = expectation.type,
            .message = fmt::format("HTTP Status validation failed (invalid parameter): {}", e.what())};
    } catch (const std::out_of_range& e) {
        return ValidationResult{.passed = false,
                                .expectation_type = expectation.type,
                                .message = fmt::format("HTTP Status validation failed (out-of-range): {}", e.what())};
    }
}

ValidationResult HttpLatencyValidation::validate(const core::TargetState& target_state,
                                                 const manifests::Expectation& expectation) const {
    try {
        return detail::performHttpValidation(
            target_state, expectation,
            [max_ms = getIntParamOrDefault(expectation, "max_latency_ms", 1000),
             min_ms = getIntParamOrDefault(expectation, "min_latency_ms", 0)](int, double elapsed_ms,
                                                                             ValidationResult& result) {
                result.passed = (elapsed_ms >= min_ms && elapsed_ms <= max_ms);
                result.message =
                    result.passed ? fmt::format("HTTP Latency {:.0f}ms (expected {} - {}ms)", elapsed_ms, min_ms, max_ms)
                                  : fmt::format("HTTP Latency {:.0f}ms outside expected bounds ({} - {}ms)", elapsed_ms,
                                                min_ms, max_ms);
            });
    } catch (const std::invalid_argument& e) {
        return ValidationResult{
            .passed = false,
            .expectation_type = expectation.type,
            .message = fmt::format("HTTP Latency validation failed (invalid parameter): {}", e.what())};
    } catch (const std::out_of_range& e) {
        return ValidationResult{.passed = false,
                                .expectation_type = expectation.type,
                                .message = fmt::format("HTTP Latency validation failed (out-of-range): {}", e.what())};
    }
}

}  // namespace chaos::orchestrator::validation
