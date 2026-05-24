/**
 * @file DockerClientInternalUnitTest.cpp
 * @brief Unit tests for extracted DockerClient internal helpers.
 */

#include <gtest/gtest.h>

#include <filesystem>

#include "containers/internal/DockerClientInternal.hpp"

using namespace chaos::orchestrator::containers::internal::detail;

/**
 * @test isWithinBuildContext rejects path traversal via ".." components.
 */
TEST(DockerClientInternalTest, RejectsDirectoryTraversal) {
    auto context = std::filesystem::temp_directory_path() / "build_context_test";
    std::filesystem::create_directories(context / "subdir");
    std::filesystem::create_directories(context / "subdir" / "deep");

    auto escaped = context / "subdir" / ".." / ".." / "etc" / "passwd";
    EXPECT_FALSE(isWithinBuildContext(escaped, context));

    auto normal = context / "subdir" / "deep";
    EXPECT_TRUE(isWithinBuildContext(normal, context));

    std::filesystem::remove_all(context);
}

/**
 * @test isWithinBuildContext accepts paths directly within the context root.
 */
TEST(DockerClientInternalTest, AcceptsPathsWithinContext) {
    auto context = std::filesystem::temp_directory_path() / "build_context_test2";
    std::filesystem::create_directories(context);

    auto inside = context / "Dockerfile";
    EXPECT_TRUE(isWithinBuildContext(inside, context));

    std::filesystem::remove_all(context);
}

/**
 * @test isWithinBuildContext accepts a path equal to the context root.
 */
TEST(DockerClientInternalTest, AcceptsContextRootItself) {
    auto context = std::filesystem::temp_directory_path() / "build_context_test3";
    std::filesystem::create_directories(context);

    EXPECT_TRUE(isWithinBuildContext(context, context));

    std::filesystem::remove_all(context);
}
