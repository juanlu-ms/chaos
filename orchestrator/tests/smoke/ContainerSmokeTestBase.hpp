#pragma once

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::tests::smoke {

inline std::shared_ptr<containers::IContainerEngine> createTestEngine() {
    return containers::createContainerEngine();
}

class ContainerSmokeTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = createTestEngine();
    }

    void TearDown() override {
        if (engine_ && !containerId_.empty()) {
            try {
                engine_->removeContainer(containerId_);
            } catch (const std::exception& ex) {
                ADD_FAILURE() << "Failed to remove container " << containerId_
                              << " during teardown: " << ex.what();
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
