/**
 * @file ContainerSmokeTestBase.hpp
 * @brief Shared test fixture base for container-related smoke tests.
 */

#pragma once

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"
#include "support/TestPreconditions.hpp"

namespace chaos::orchestrator::tests::smoke {

/** @brief Creates a real container engine instance for smoke testing. @return A shared pointer to the engine. */
inline std::shared_ptr<containers::IContainerEngine> createTestEngine() { return containers::createContainerEngine(); }

/**
 * @brief Base test fixture that provisions a demo-target container for each test case.
 *
 * The tests are skipped when Docker is not available. A fixture that overrides SetUp() must
 * return right after calling this one if IsSkipped() or HasFatalFailure() is true: GTEST_SKIP()
 * only leaves the function it is written in, so the derived SetUp() would carry on with no engine.
 */
class ContainerSmokeTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        // Everything here goes through the Docker API, which the docker group can use without root.
        CHAOS_REQUIRE_TEST_ENVIRONMENT(support::Requirement::Docker);
        engine_ = createTestEngine();
        engine_->buildImage("chaos-demo-target:latest", CHAOS_EXAMPLES_DIR "/demo-target/Dockerfile");
    }

    void TearDown() override {
        if (engine_ && !containerId_.empty()) {
            try {
                engine_->removeContainer(containerId_);
            } catch (const std::exception& ex) {
                ADD_FAILURE() << "Failed to remove container " << containerId_ << " during teardown: " << ex.what();
            }
            containerId_.clear();
        }
    }

    void createAndStartContainer() {
        containerId_ = engine_->createContainer("chaos-demo-target:latest", {});
        ASSERT_FALSE(containerId_.empty());
        engine_->startContainer(containerId_);
    }

    std::shared_ptr<containers::IContainerEngine> engine_;
    std::string containerId_;
};

}  // namespace chaos::orchestrator::tests::smoke
