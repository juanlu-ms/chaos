/// @file ValidationEngineUnitTest.cpp
/// @brief Unit tests for ValidationEngine.

#include <gtest/gtest.h>

#include <memory>
#include <string>
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

TEST(ValidationEngineTests, UnknownExpectationTypeReturnsFailed) {
    auto mock = std::make_shared<tests::MockContainerEngine>();

    auto engine = makeEngine(mock);
    manifests::Expectation exp{"unknown_type", {}};
    const auto results = engine.validate("ctr", {exp});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].passed);
    EXPECT_EQ(results[0].expectationType, "unknown_type");
}
