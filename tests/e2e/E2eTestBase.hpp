/**
 * @file E2eTestBase.hpp
 * @brief Shared test fixture base for end-to-end integration tests.
 */

#pragma once

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/ContainerStatus.hpp"
#include "containers/IContainerEngine.hpp"

using chaos::orchestrator::containers::ContainerStatus;
using chaos::orchestrator::containers::createContainerEngine;
using chaos::orchestrator::containers::IContainerEngine;

/**
 * @brief Base test fixture that provisions a running demo-target container for each E2E test case.
 */
class E2eTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = createContainerEngine();
        engine_->buildImage("chaos-demo-target:latest", CHAOS_EXAMPLES_DIR "/demo-target/Dockerfile");
        containerId_ = engine_->createContainer("chaos-demo-target:latest", {});
        ASSERT_FALSE(containerId_.empty());
        engine_->startContainer(containerId_);
        ASSERT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);
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

    /** @brief Returns the container engine instance. @return Shared pointer to the engine. */
    std::shared_ptr<IContainerEngine> engine() const { return engine_; }
    /** @brief Returns the running container's identifier. @return Const reference to the container ID string. */
    [[nodiscard]] const std::string& containerId() const { return containerId_; }

private:
    std::shared_ptr<IContainerEngine> engine_;
    std::string containerId_;
};
