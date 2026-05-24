/**
 * @file ContainerEngineFactoryUnitTest.cpp
 * @brief Unit tests for ContainerEngineFactory.
 */

#include <gtest/gtest.h>

#include "containers/ContainerEngineFactory.hpp"

using namespace chaos::orchestrator;

/**
 * @test Verifies the factory creates an engine with the default socket path.
 */
TEST(ContainerEngineFactoryUnitTest, CreatesEngineWithDefaultSocketPath) {
    auto engine = containers::createContainerEngine();
    EXPECT_NE(engine, nullptr);
}

/**
 * @test Verifies the factory creates an engine with a custom socket path.
 */
TEST(ContainerEngineFactoryUnitTest, CreatesEngineWithCustomSocketPath) {
    auto engine = containers::createContainerEngine("/tmp/docker.sock");
    EXPECT_NE(engine, nullptr);
}
