#include <gtest/gtest.h>

#include "ContainerSmokeTestBase.hpp"

using namespace chaos::orchestrator::tests::smoke;

class ContainerResourceTest : public ContainerSmokeTestBase {
protected:
    void SetUp() override {
        ContainerSmokeTestBase::SetUp();
        createAndStartContainer();
    }
};

TEST_F(ContainerResourceTest, UpdateMemoryLimitDoesNotThrow) {
    EXPECT_NO_THROW(engine_->updateMemoryLimit(containerId_, 128 * 1024 * 1024));
}

TEST_F(ContainerResourceTest, UpdateMemoryLimitZeroResets) {
    ASSERT_NO_THROW(engine_->updateMemoryLimit(containerId_, 128 * 1024 * 1024));
    EXPECT_NO_THROW(engine_->updateMemoryLimit(containerId_, 0));
}

TEST_F(ContainerResourceTest, UpdateCpuQuotaDoesNotThrow) {
    EXPECT_NO_THROW(engine_->updateCpuQuota(containerId_, 50000, 100000));
}

TEST_F(ContainerResourceTest, UpdateCpuQuotaResetDoesNotThrow) {
    ASSERT_NO_THROW(engine_->updateCpuQuota(containerId_, 50000, 100000));
    EXPECT_NO_THROW(engine_->updateCpuQuota(containerId_, -1, 0));
}

TEST_F(ContainerResourceTest, GetContainerMemoryUsageReturnsNonNegative) {
    const auto stats = engine_->getStats(containerId_);
    EXPECT_TRUE(stats.memory_mb.has_value());
    EXPECT_GT(*stats.memory_mb, 0.0) << "Running container should consume some memory";
}

TEST_F(ContainerResourceTest, GetContainerCpuUsageReturnsNonNegative) {
    const auto stats = engine_->getStats(containerId_);
    EXPECT_TRUE(stats.cpu_percent.has_value());
    EXPECT_GE(*stats.cpu_percent, 0.0);
}
