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

/**
 * @test Verifies isRunning returns true when the container is matched by its name.
 * Docker API returns names with a leading '/', so the engine should match both
 * the plain name ("my-app") and the Docker-prefixed name ("/my-app") against a
 * plain lookup key.
 */
TEST(ObservabilityEngineTests, IsRunningReturnsTrueWhenContainerMatchedByName) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();

    // Case 1: Docker name without slash — lookup by plain name matches
    {
        containers::Container c;
        c.id = "abc123fullid";
        c.name = "my-app";
        c.state = "running";
        EXPECT_CALL(*mockEngine, listContainers())
            .WillRepeatedly(Return(std::vector<containers::Container>{c}));
        observability::ObservabilityEngine obs(mockEngine);
        EXPECT_TRUE(obs.isRunning("my-app"));
    }

    // Case 2: Docker name with leading slash — lookup by plain name still matches
    {
        auto mockEngine2 = std::make_shared<tests::MockContainerEngine>();
        containers::Container c;
        c.id = "abc123fullid";
        c.name = "/my-app";  // Docker API often returns slash-prefixed names
        c.state = "running";
        EXPECT_CALL(*mockEngine2, listContainers())
            .WillRepeatedly(Return(std::vector<containers::Container>{c}));
        observability::ObservabilityEngine obs(mockEngine2);
        EXPECT_TRUE(obs.isRunning("my-app"));
    }
}
