/// @file ValidationEngineUnitTest.cpp
/// @brief Unit tests for ValidationEngine.

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "MockContainerEngine.hpp"
#include "containers/Container.hpp"
#include "manifests/Manifest.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "observability/ValidationEngine.hpp"

using namespace testing;
using namespace chaos::orchestrator;

namespace {

observability::ValidationEngine makeEngine(std::shared_ptr<tests::MockContainerEngine> mock) {
    return observability::ValidationEngine(observability::ObservabilityEngine(std::move(mock)));
}

containers::Container makeContainer(std::string id, std::string state) {
    containers::Container c;
    c.id = std::move(id);
    c.state = std::move(state);
    return c;
}

}  // namespace

TEST(ValidationEngineTests, ContainerRunningPassesWhenRunning) {
    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, listContainers())
        .WillOnce(Return(std::vector<containers::Container>{makeContainer("ctr", "running")}));

    auto engine = makeEngine(mock);
    const auto results = engine.validate("ctr", {manifests::Expectation{"container_running", {}}});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
    EXPECT_EQ(results[0].expectationType, "container_running");
}

TEST(ValidationEngineTests, ContainerRunningFailsWhenNotRunning) {
    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, listContainers())
        .WillOnce(Return(std::vector<containers::Container>{makeContainer("ctr", "exited")}));

    auto engine = makeEngine(mock);
    const auto results = engine.validate("ctr", {manifests::Expectation{"container_running", {}}});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

TEST(ValidationEngineTests, ContainerNotRunningPassesWhenStopped) {
    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, listContainers())
        .WillOnce(Return(std::vector<containers::Container>{makeContainer("ctr", "exited")}));

    auto engine = makeEngine(mock);
    const auto results = engine.validate("ctr", {manifests::Expectation{"container_not_running", {}}});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

TEST(ValidationEngineTests, ContainerNotRunningFailsWhenRunning) {
    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, listContainers())
        .WillOnce(Return(std::vector<containers::Container>{makeContainer("ctr", "running")}));

    auto engine = makeEngine(mock);
    const auto results = engine.validate("ctr", {manifests::Expectation{"container_not_running", {}}});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

TEST(ValidationEngineTests, LogContainsPassesWhenSubstringPresent) {
    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, getLogs(std::string_view("ctr"))).WillOnce(Return(std::string{"app started ok\n"}));

    auto engine = makeEngine(mock);
    manifests::Expectation exp{"log_contains", {{"substring", "started"}}};
    const auto results = engine.validate("ctr", {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

TEST(ValidationEngineTests, LogContainsFailsWhenSubstringAbsent) {
    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, getLogs(std::string_view("ctr"))).WillOnce(Return(std::string{"crash\n"}));

    auto engine = makeEngine(mock);
    manifests::Expectation exp{"log_contains", {{"substring", "started"}}};
    const auto results = engine.validate("ctr", {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

TEST(ValidationEngineTests, LogNotContainsPassesWhenSubstringAbsent) {
    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, getLogs(std::string_view("ctr"))).WillOnce(Return(std::string{"all good\n"}));

    auto engine = makeEngine(mock);
    manifests::Expectation exp{"log_not_contains", {{"substring", "panic"}}};
    const auto results = engine.validate("ctr", {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

TEST(ValidationEngineTests, LogNotContainsFailsWhenSubstringPresent) {
    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, getLogs(std::string_view("ctr"))).WillRepeatedly(Return(std::string{"panic: nil ptr\n"}));

    auto engine = makeEngine(mock);
    manifests::Expectation exp{"log_not_contains", {{"substring", "panic"}}};
    const auto results = engine.validate("ctr", {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

TEST(ValidationEngineTests, MixedExpectationsReturnsAllResults) {
    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, listContainers())
        .WillOnce(Return(std::vector<containers::Container>{makeContainer("ctr", "running")}));
    EXPECT_CALL(*mock, getLogs(std::string_view("ctr"))).WillOnce(Return(std::string{"app started\n"}));

    auto engine = makeEngine(mock);
    const std::vector<manifests::Expectation> expectations = {
        {"container_running", {}},
        {"log_contains", {{"substring", "started"}}},
    };
    const auto results = engine.validate("ctr", expectations);

    ASSERT_EQ(results.size(), 2u);
    EXPECT_TRUE(results[0].passed);
    EXPECT_EQ(results[0].expectationType, "container_running");
    EXPECT_TRUE(results[1].passed);
    EXPECT_EQ(results[1].expectationType, "log_contains");
}

TEST(ValidationEngineTests, UnknownExpectationTypeThrowsInvalidArgument) {
    auto mock = std::make_shared<tests::MockContainerEngine>();

    auto engine = makeEngine(mock);
    manifests::Expectation exp{"unknown_type", {}};
    EXPECT_THROW(static_cast<void>(engine.validate("ctr", {exp})), std::invalid_argument);
}

TEST(ValidationEngineTests, HttpStatusPassesOnExpectedStatus) {
    httplib::Server svr;
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content("ok", "text/plain");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::jthread t([&svr]() { svr.listen_after_bind(); });

    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, getContainerIp(std::string_view("ctr"))).WillOnce(Return("127.0.0.1"));

    auto engine = makeEngine(mock);
    manifests::Expectation exp{"http_status",
                               {{"port", std::to_string(port)}, {"path", "/ping"}, {"expected_status", "200"}}};
    const auto results = engine.validate("ctr", {exp});

    svr.stop();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].passed);
}

TEST(ValidationEngineTests, HttpStatusFailsOnUnexpectedStatus) {
    httplib::Server svr;
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res) {
        res.status = 404;
        res.set_content("not found", "text/plain");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::jthread t([&svr]() { svr.listen_after_bind(); });

    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, getContainerIp(std::string_view("ctr"))).WillOnce(Return("127.0.0.1"));

    auto engine = makeEngine(mock);
    manifests::Expectation exp{"http_status",
                               {{"port", std::to_string(port)}, {"path", "/ping"}, {"expected_status", "200"}}};
    const auto results = engine.validate("ctr", {exp});

    svr.stop();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
}

TEST(ValidationEngineTests, HttpLatencyValidatesBounds) {
    httplib::Server svr;
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        res.status = 200;
        res.set_content("ok", "text/plain");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::jthread t([&svr]() { svr.listen_after_bind(); });

    auto mock = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mock, getContainerIp(std::string_view("ctr"))).WillRepeatedly(Return("127.0.0.1"));

    auto engine = makeEngine(mock);
    manifests::Expectation exp_pass{
        "http_latency",
        {{"port", std::to_string(port)}, {"path", "/ping"}, {"min_latency_ms", "0"}, {"max_latency_ms", "500"}}};
    const auto results_pass = engine.validate("ctr", {exp_pass});
    ASSERT_EQ(results_pass.size(), 1u);
    EXPECT_TRUE(results_pass[0].passed);

    manifests::Expectation exp_fail{
        "http_latency",
        {{"port", std::to_string(port)}, {"path", "/ping"}, {"min_latency_ms", "0"}, {"max_latency_ms", "10"}}};
    const auto results_fail = engine.validate("ctr", {exp_fail});
    ASSERT_EQ(results_fail.size(), 1u);
    EXPECT_FALSE(results_fail[0].passed);

    svr.stop();
}
