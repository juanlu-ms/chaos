/**
 * @file HttpValidationUnitTest.cpp
 * @brief Unit tests for HttpStatusValidation and HttpLatencyValidation.
 */

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "core/TargetState.hpp"
#include "manifests/Manifest.hpp"
#include "validation/internal/HttpValidation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::validation;

namespace {

core::TargetState makeState(std::optional<std::string> ip) {
    core::TargetState s;
    s.container_id = "test-ctr";
    s.status = containers::ContainerStatus::Running;
    s.container_ip = std::move(ip);
    return s;
}

}  // namespace

/**
 * @test Verifies HttpStatusValidation fails when container IP is missing.
 */
TEST(HttpValidationTest, StatusFailsWhenIpMissing) {
    HttpStatusValidation v;
    auto result = v.validate(makeState(std::nullopt),
                             {"http_status", {{"port", "8080"}, {"path", "/ping"}, {"expected_status", "200"}}});
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.expectation_type, "http_status");
}

/**
 * @test Verifies HttpStatusValidation fails on invalid port parameter.
 */
TEST(HttpValidationTest, StatusFailsOnInvalidPort) {
    HttpStatusValidation v;
    auto result = v.validate(makeState("127.0.0.1"),
                             {"http_status", {{"port", "bad"}, {"path", "/ping"}, {"expected_status", "200"}}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies HttpLatencyValidation fails when container IP is missing.
 */
TEST(HttpValidationTest, LatencyFailsWhenIpMissing) {
    HttpLatencyValidation v;
    auto result = v.validate(makeState(std::nullopt),
                             {"http_latency", {{"port", "8080"}, {"path", "/ping"}, {"max_latency_ms", "100"}}});
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.expectation_type, "http_latency");
}

/**
 * @test Verifies HttpLatencyValidation fails on invalid max_latency_ms parameter.
 */
TEST(HttpValidationTest, LatencyFailsOnInvalidMaxLatency) {
    HttpLatencyValidation v;
    auto result = v.validate(makeState("127.0.0.1"),
                             {"http_latency", {{"port", "8080"}, {"path", "/ping"}, {"max_latency_ms", "bad"}}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies HttpLatencyValidation fails when min_latency_ms exceeds max_latency_ms.
 */
TEST(HttpValidationTest, LatencyFailsWhenMinExceedsMax) {
    HttpLatencyValidation v;
    auto result = v.validate(
        makeState("127.0.0.1"),
        {"http_latency", {{"port", "8080"}, {"path", "/ping"}, {"min_latency_ms", "500"}, {"max_latency_ms", "100"}}});
    EXPECT_FALSE(result.passed);
}
