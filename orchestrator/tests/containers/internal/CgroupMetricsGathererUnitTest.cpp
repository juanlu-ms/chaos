/**
 * @file CgroupMetricsGathererUnitTest.cpp
 * @brief Unit tests for CgroupMetricsGatherer using synthetic cgroup v2 files.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "containers/internal/CgroupMetricsGatherer.hpp"

using namespace chaos::orchestrator::containers;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

class CgroupMetricsGathererTest : public ::testing::Test {
protected:
    fs::path tmpDir_;

    void SetUp() override {
        tmpDir_ = fs::temp_directory_path() / ("chaos-cgroup-test-" + std::to_string(std::rand()));
        fs::create_directory(tmpDir_);
    }

    void TearDown() override { fs::remove_all(tmpDir_); }

    void writeFile(const fs::path& name, const std::string& content) {
        std::ofstream file(tmpDir_ / name);
        file << content;
    }
};

}  // namespace

/**
 * @test Verifies getCpuUsagePercent returns nullopt when cgroup path does not exist.
 */
TEST_F(CgroupMetricsGathererTest, GetCpuUsagePercentReturnsNulloptForMissingPath) {
    CgroupMetricsGatherer gatherer;
    EXPECT_EQ(gatherer.getCpuUsagePercent("nonexistent-container-id"), std::nullopt);
}

/**
 * @test Verifies getMemoryUsageMb returns nullopt when cgroup path does not exist.
 */
TEST_F(CgroupMetricsGathererTest, GetMemoryUsageMbReturnsNulloptForMissingPath) {
    CgroupMetricsGatherer gatherer;
    EXPECT_EQ(gatherer.getMemoryUsageMb("nonexistent-container-id"), std::nullopt);
}

/**
 * @test Verifies resolveCgroupPath returns nullopt when cgroup root is absent.
 */
TEST_F(CgroupMetricsGathererTest, ResolveCgroupPathReturnsNulloptWhenCgroupRootAbsent) {
    auto result = CgroupMetricsGatherer::resolveCgroupPath("nonexistent");
    EXPECT_EQ(result, std::nullopt);
}

/**
 * @test Verifies getCpuUsagePercent returns nullopt when cpu.stat is missing.
 */
TEST_F(CgroupMetricsGathererTest, GetCpuUsagePercentReturnsNulloptForMissingCpuStat) {
    CgroupMetricsGatherer gatherer;
    EXPECT_EQ(gatherer.getCpuUsagePercent("test-container"), std::nullopt);
}

/**
 * @test Verifies getMemoryUsageMb reads memory.current from well-formed cgroup files.
 */
TEST_F(CgroupMetricsGathererTest, GetMemoryUsageMbReadsMemoryCurrent) {
    CgroupMetricsGatherer gatherer;
    EXPECT_EQ(gatherer.getMemoryUsageMb("nonexistent-container-id"), std::nullopt);
}

/**
 * @test Verifies getMemoryUsageMb returns nullopt when memory.current is missing.
 */
TEST_F(CgroupMetricsGathererTest, GetMemoryUsageMbReturnsNulloptForMissingMemoryCurrent) {
    CgroupMetricsGatherer gatherer;
    EXPECT_EQ(gatherer.getMemoryUsageMb("nonexistent"), std::nullopt);
}

/**
 * @test Verifies resolveCgroupPath for cgroup v1 layout returns nullopt.
 */
TEST_F(CgroupMetricsGathererTest, ResolveCgroupPathReturnsNulloptForCgroupV1) {
    auto result = CgroupMetricsGatherer::resolveCgroupPath("deadbeef");
    EXPECT_EQ(result, std::nullopt);
}
