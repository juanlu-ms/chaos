/// @file ObservabilityEngineUnitTest.cpp
/// @brief Unit tests for ObservabilityEngine.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "MockContainerEngine.hpp"
#include "containers/Container.hpp"
#include "observability/ObservabilityEngine.hpp"

using namespace testing;
using namespace chaos::orchestrator;

TEST(ObservabilityEngineTests, IsRunningReturnsTrueWhenContainerRunning) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    containers::Container c;
    c.id = "abc123";
    c.state = "running";

    EXPECT_CALL(*mockEngine, listContainers()).WillOnce(Return(std::vector<containers::Container>{c}));

    observability::ObservabilityEngine obs(mockEngine);
    EXPECT_TRUE(obs.isRunning("abc123"));
}

TEST(ObservabilityEngineTests, IsRunningReturnsFalseWhenContainerExited) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    containers::Container c;
    c.id = "abc123";
    c.state = "exited";

    EXPECT_CALL(*mockEngine, listContainers()).WillOnce(Return(std::vector<containers::Container>{c}));

    observability::ObservabilityEngine obs(mockEngine);
    EXPECT_FALSE(obs.isRunning("abc123"));
}

TEST(ObservabilityEngineTests, IsRunningReturnsFalseWhenContainerNotInList) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, listContainers()).WillOnce(Return(std::vector<containers::Container>{}));

    observability::ObservabilityEngine obs(mockEngine);
    EXPECT_FALSE(obs.isRunning("nonexistent"));
}

TEST(ObservabilityEngineTests, GetLogsDelegatesToEngine) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, getLogs(std::string_view("abc123")))
        .WillOnce(Return(std::string{"log line 1\nlog line 2\n"}));

    observability::ObservabilityEngine obs(mockEngine);
    EXPECT_EQ(obs.getLogs("abc123"), "log line 1\nlog line 2\n");
}
