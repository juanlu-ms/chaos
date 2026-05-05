#include <gtest/gtest.h>

#include "ContainerSmokeTestBase.hpp"

using namespace chaos::orchestrator::tests::smoke;

class ContainerNetworkTest : public ContainerSmokeTestBase {
protected:
    void SetUp() override {
        ContainerSmokeTestBase::SetUp();
        createAndStartContainer();
    }
};

TEST_F(ContainerNetworkTest, GetContainerIpReturnsValidAddress) {
    const auto ip = engine_->getContainerIp(containerId_);
    EXPECT_FALSE(ip.empty());
    EXPECT_NE(ip, "0.0.0.0");
}
