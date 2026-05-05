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
    const auto mem = engine_->getContainerMemoryUsage(containerId_);
    EXPECT_GT(mem, 0.0) << "Running container should consume some memory";
}

TEST_F(ContainerResourceTest, GetContainerCpuUsageReturnsNonNegative) {
    const auto cpu = engine_->getContainerCpuUsage(containerId_);
    EXPECT_GE(cpu, 0.0);
}
