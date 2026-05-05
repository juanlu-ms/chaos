/// @file ValidationEngineUnitTest.cpp
/// @brief Unit tests for ValidationEngine.

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "manifests/Manifest.hpp"
#include "validation/ValidationEngine.hpp"

using namespace testing;
using namespace chaos::orchestrator;

namespace {

shared::TargetState makeTargetState(std::string id, shared::ContainerStatus status) {
    shared::TargetState state;
    state.container_id = std::move(id);
    state.status = status;
    return state;
}

}  // namespace

/**
 * @test Verifies container_running passes when status is running.
 */
TEST(ValidationEngineTests, ContainerRunningPassesWhenRunning) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    const auto results = validation::ValidationEngine::validate(state,
                                                                {manifests::Expectation{"container_running", {}}});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
    EXPECT_EQ(results[0].expectationType, "container_running");
}

/**
 * @test Verifies container_running fails when status is not running.
 */
TEST(ValidationEngineTests, ContainerRunningFailsWhenNotRunning) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Exited);
    const auto results = validation::ValidationEngine::validate(state,
                                                                {manifests::Expectation{"container_running", {}}});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies container_not_running passes when status is not running.
 */
TEST(ValidationEngineTests, ContainerNotRunningPassesWhenStopped) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Exited);
    const auto results = validation::ValidationEngine::validate(state,
                                                                {manifests::Expectation{"container_not_running", {}}});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

/**
 * @test Verifies container_not_running fails when status is running.
 */
TEST(ValidationEngineTests, ContainerNotRunningFailsWhenRunning) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    const auto results = validation::ValidationEngine::validate(state,
                                                                {manifests::Expectation{"container_not_running", {}}});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies log_contains passes when substring is present.
 */
TEST(ValidationEngineTests, LogContainsPassesWhenSubstringPresent) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.recent_logs = {"app started ok\n"};
    manifests::Expectation exp{"log_contains", {{"substring", "started"}}};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

/**
 * @test Verifies log_contains fails when substring is absent.
 */
TEST(ValidationEngineTests, LogContainsFailsWhenSubstringAbsent) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.recent_logs = {"crash\n"};
    manifests::Expectation exp{"log_contains", {{"substring", "started"}}};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies log_not_contains passes when substring is absent.
 */
TEST(ValidationEngineTests, LogNotContainsPassesWhenSubstringAbsent) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.recent_logs = {"all good\n"};
    manifests::Expectation exp{"log_not_contains", {{"substring", "panic"}}};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

/**
 * @test Verifies log_not_contains fails when substring is present.
 */
TEST(ValidationEngineTests, LogNotContainsFailsWhenSubstringPresent) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.recent_logs = {"panic: nil ptr\n"};
    manifests::Expectation exp{"log_not_contains", {{"substring", "panic"}}};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies validate returns results for all expectations in order.
 */
TEST(ValidationEngineTests, MixedExpectationsReturnsAllResults) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.recent_logs = {"app started\n"};
    const std::vector<manifests::Expectation> expectations = {
        {"container_running", {}},
        {"log_contains", {{"substring", "started"}}},
    };
    const auto results = validation::ValidationEngine::validate(state, expectations);

    ASSERT_EQ(results.size(), 2u);
    EXPECT_TRUE(results[0].passed);
    EXPECT_EQ(results[0].expectationType, "container_running");
    EXPECT_TRUE(results[1].passed);
    EXPECT_EQ(results[1].expectationType, "log_contains");
}

/**
 * @test Verifies unknown expectation types throw std::invalid_argument.
 */
TEST(ValidationEngineTests, UnknownExpectationTypeThrowsInvalidArgument) {
    manifests::Expectation exp{"unknown_type", {}};
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    EXPECT_THROW(static_cast<void>(validation::ValidationEngine::validate(state, {exp})), std::invalid_argument);
}

/**
 * @test Verifies http_status passes when the response status matches.
 */
TEST(ValidationEngineTests, HttpStatusPassesOnExpectedStatus) {
    httplib::Server svr;
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content("ok", "text/plain");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::jthread t([&svr]() { svr.listen_after_bind(); });

    manifests::Expectation exp{"http_status",
                               {{"port", std::to_string(port)}, {"path", "/ping"}, {"expected_status", "200"}}};
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.container_ip = std::string{"127.0.0.1"};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    svr.stop();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

/**
 * @test Verifies http_status fails when the response status mismatches.
 */
TEST(ValidationEngineTests, HttpStatusFailsOnUnexpectedStatus) {
    httplib::Server svr;
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res) {
        res.status = 404;
        res.set_content("not found", "text/plain");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::jthread t([&svr]() { svr.listen_after_bind(); });

    manifests::Expectation exp{"http_status",
                               {{"port", std::to_string(port)}, {"path", "/ping"}, {"expected_status", "200"}}};
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.container_ip = std::string{"127.0.0.1"};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    svr.stop();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies http_latency checks both min and max bounds.
 */
TEST(ValidationEngineTests, HttpLatencyValidatesBounds) {
    httplib::Server svr;
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        res.status = 200;
        res.set_content("ok", "text/plain");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::jthread t([&svr]() { svr.listen_after_bind(); });

    manifests::Expectation exp_pass{
        "http_latency",
        {{"port", std::to_string(port)}, {"path", "/ping"}, {"min_latency_ms", "0"}, {"max_latency_ms", "500"}}};
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.container_ip = std::string{"127.0.0.1"};
    const auto results_pass = validation::ValidationEngine::validate(state, {exp_pass});
    ASSERT_EQ(results_pass.size(), 1u);
    EXPECT_TRUE(results_pass[0].passed);

    manifests::Expectation exp_fail{
        "http_latency",
        {{"port", std::to_string(port)}, {"path", "/ping"}, {"min_latency_ms", "0"}, {"max_latency_ms", "10"}}};
    const auto results_fail = validation::ValidationEngine::validate(state, {exp_fail});
    ASSERT_EQ(results_fail.size(), 1u);
    EXPECT_FALSE(results_fail[0].passed);

    svr.stop();
}

/**
 * @test Verifies http_status fails when container IP is unavailable.
 */
TEST(ValidationEngineTests, HttpStatusFailsWhenIpMissing) {
    manifests::Expectation exp{"http_status", {{"port", "8080"}, {"path", "/ping"}, {"expected_status", "200"}}};
    shared::TargetState state;
    state.container_id = "ctr";
    state.container_ip = std::nullopt;
    state.status = shared::ContainerStatus::Running;

    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies http_status fails for invalid port parameters.
 */
TEST(ValidationEngineTests, HttpStatusHandlesInvalidPortParameter) {
    manifests::Expectation exp{"http_status", {{"port", "not-a-number"}, {"path", "/ping"}, {"expected_status", "200"}}};
    shared::TargetState state;
    state.container_id = "ctr";
    state.container_ip = std::string{"127.0.0.1"};
    state.status = shared::ContainerStatus::Running;

    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies http_latency fails for invalid latency parameters.
 */
TEST(ValidationEngineTests, HttpLatencyHandlesInvalidLatencyParameter) {
    manifests::Expectation exp{"http_latency", {{"port", "8080"}, {"path", "/ping"}, {"max_latency_ms", "oops"}}};
    shared::TargetState state;
    state.container_id = "ctr";
    state.container_ip = std::string{"127.0.0.1"};
    state.status = shared::ContainerStatus::Running;

    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies http_latency fails when container IP is unavailable.
 */
TEST(ValidationEngineTests, HttpLatencyFailsWhenIpMissing) {
    manifests::Expectation exp{"http_latency", {{"port", "8080"}, {"path", "/ping"}, {"max_latency_ms", "50"}}};
    shared::TargetState state;
    state.container_id = "ctr";
    state.container_ip = std::nullopt;
    state.status = shared::ContainerStatus::Running;

    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies log_contains passes when substring parameter is empty (empty string is trivially present).
 */
TEST(ValidationEngineTests, LogContainsPassesWithEmptySubstring) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.recent_logs = {"some log\n"};
    manifests::Expectation exp{"log_contains", {{"substring", ""}}};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

/**
 * @test Verifies log_not_contains fails when substring parameter is empty (empty string is trivially contained).
 */
TEST(ValidationEngineTests, LogNotContainsFailsWithEmptySubstring) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.recent_logs = {"some log\n"};
    manifests::Expectation exp{"log_not_contains", {{"substring", ""}}};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies log_contains searches across multiple log lines.
 */
TEST(ValidationEngineTests, LogContainsSearchesAllLines) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    state.recent_logs = {"first line\n", "second line\n", "third line\n"};
    manifests::Expectation exp{"log_contains", {{"substring", "second"}}};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

/**
 * @test Verifies log expectations handle empty logs gracefully.
 */
TEST(ValidationEngineTests, LogExpectationsHandleEmptyLogs) {
    auto state = makeTargetState("ctr", shared::ContainerStatus::Running);
    manifests::Expectation exp{"log_contains", {{"substring", "missing"}}};
    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

/**
 * @test Verifies http_latency fails when min_latency_ms exceeds max_latency_ms.
 */
TEST(ValidationEngineTests, HttpLatencyFailsWhenMinExceedsMax) {
    manifests::Expectation exp{"http_latency", {{"port", "8080"}, {"path", "/ping"}, {"min_latency_ms", "500"}, {"max_latency_ms", "100"}}};
    shared::TargetState state;
    state.container_id = "ctr";
    state.container_ip = std::string{"127.0.0.1"};
    state.status = shared::ContainerStatus::Running;

    const auto results = validation::ValidationEngine::validate(state, {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}
