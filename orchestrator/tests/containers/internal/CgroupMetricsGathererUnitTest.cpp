/**
 * @file CgroupMetricsGathererUnitTest.cpp
 * @brief Unit tests for CgroupMetricsGatherer error paths.
 */

#include <gtest/gtest.h>

#include <optional>

#include "containers/internal/CgroupMetricsGatherer.hpp"

using namespace chaos::orchestrator::containers;

/**
 * @test Verifies getCpuUsagePercent returns nullopt when cgroup path does not exist.
 */
TEST(CgroupMetricsGathererTest, GetCpuUsagePercentReturnsNulloptForMissingPath) {
    CgroupMetricsGatherer gatherer;
    EXPECT_EQ(gatherer.getCpuUsagePercent("nonexistent-container-id"), std::nullopt);
}

/**
 * @test Verifies getMemoryUsageMb returns nullopt when cgroup path does not exist.
 */
TEST(CgroupMetricsGathererTest, GetMemoryUsageMbReturnsNulloptForMissingPath) {
    CgroupMetricsGatherer gatherer;
    EXPECT_EQ(gatherer.getMemoryUsageMb("nonexistent-container-id"), std::nullopt);
}

/**
 * @test Verifies resolveCgroupPath returns nullopt when no directory matches the container ID.
 */
TEST(CgroupMetricsGathererTest, ResolveCgroupPathReturnsNulloptForUnknownId) {
    auto result = CgroupMetricsGatherer::resolveCgroupPath("nonexistent");
    EXPECT_EQ(result, std::nullopt);
}

/**
 * @test Verifies resolveCgroupPath returns nullopt for an ID that won't match any cgroup directory.
 */
TEST(CgroupMetricsGathererTest, ResolveCgroupPathReturnsNulloptForUnlikelyId) {
    auto result = CgroupMetricsGatherer::resolveCgroupPath("deadbeef12345");
    EXPECT_EQ(result, std::nullopt);
}
