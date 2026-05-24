/**
 * @file ContainerNetworkTest.cpp
 * @brief Smoke tests for container network operations.
 */

#include <gtest/gtest.h>

#include <regex>

#include "ContainerSmokeTestBase.hpp"

using namespace chaos::orchestrator::tests::smoke;

class ContainerNetworkTest : public ContainerSmokeTestBase {
protected:
    void SetUp() override {
        ContainerSmokeTestBase::SetUp();
        createAndStartContainer();
    }
};

/**
 * @test Verifies getContainerIp returns a valid IPv4 address.
 */
TEST_F(ContainerNetworkTest, GetContainerIpReturnsValidAddress) {
    const auto ip = engine_->getContainerIp(containerId_);
    EXPECT_FALSE(ip.empty());
    std::regex ipv4_re(R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$)");
    EXPECT_TRUE(std::regex_match(ip, ipv4_re)) << "Invalid IPv4: " << ip;
}
