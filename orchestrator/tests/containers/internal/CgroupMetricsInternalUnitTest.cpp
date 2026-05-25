/**
 * @file CgroupMetricsInternalUnitTest.cpp
 * @brief Unit tests for cgroup v2 file reading helpers.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "containers/internal/CgroupMetricsDetail.hpp"

using namespace chaos::orchestrator::containers::detail;

/**
 * @test readSimpleU64 reads a single integer from a file.
 */
TEST(CgroupMetricsInternalTest, ReadSimpleU64FromFile) {
    auto tmpDir = std::filesystem::temp_directory_path();
    auto testFile = tmpDir / "cgroup_test_simple_u64";

    std::ofstream ofs(testFile);
    ASSERT_TRUE(ofs.good());
    ofs << "12345\n";
    ofs.close();

    auto value = readSimpleU64(testFile);
    EXPECT_TRUE(value.has_value());
    if (value) {
        EXPECT_EQ(*value, 12345U);
    }

    std::filesystem::remove(testFile);
}

/**
 * @test readSimpleU64 returns nullopt for a missing file.
 */
TEST(CgroupMetricsInternalTest, ReadSimpleU64MissingFileReturnsNullopt) {
    auto value = readSimpleU64("/nonexistent/cgroup/file");
    EXPECT_FALSE(value.has_value());
}

/**
 * @test readKeyValueU64 extracts a value by key from multi-field content.
 */
TEST(CgroupMetricsInternalTest, ReadKeyValueU64ExtractsByKey) {
    auto tmpDir = std::filesystem::temp_directory_path();
    auto testFile = tmpDir / "cgroup_test_key_value";

    std::ofstream ofs(testFile);
    ASSERT_TRUE(ofs.good());
    ofs << "usage_usec 12345\n";
    ofs << "nr_periods 100\n";
    ofs << "nr_throttled 5\n";
    ofs.close();

    auto value = readKeyValueU64(testFile, "usage_usec");
    EXPECT_TRUE(value.has_value());
    if (value) {
        EXPECT_EQ(*value, 12345U);
    }

    std::filesystem::remove(testFile);
}

/**
 * @test readKeyValueU64 returns nullopt for a missing key.
 */
TEST(CgroupMetricsInternalTest, ReadKeyValueU64MissingKeyReturnsNullopt) {
    auto tmpDir = std::filesystem::temp_directory_path();
    auto testFile = tmpDir / "cgroup_test_missing_key";

    std::ofstream ofs(testFile);
    ASSERT_TRUE(ofs.good());
    ofs << "usage_usec 12345\n";
    ofs.close();

    auto value = readKeyValueU64(testFile, "nr_periods");
    EXPECT_FALSE(value.has_value());

    std::filesystem::remove(testFile);
}

/**
 * @test readKeyValueU64 returns nullopt for a missing file.
 */
TEST(CgroupMetricsInternalTest, ReadKeyValueU64MissingFileReturnsNullopt) {
    auto value = readKeyValueU64("/nonexistent/cgroup/file", "usage_usec");
    EXPECT_FALSE(value.has_value());
}
