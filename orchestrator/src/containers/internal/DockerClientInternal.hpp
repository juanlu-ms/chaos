/**
 * @file DockerClientInternal.hpp
 * @brief Testable helpers for Docker build-context tar creation and path validation.
 */

#pragma once

#include <filesystem>

namespace chaos::orchestrator::containers::internal::detail {

// Check whether a path lies within the build context root.
// Ensures the path does not escape the build context directory via ".."
// components or symlinks. Used to prevent directory traversal in tar archives.
[[nodiscard]] bool isWithinBuildContext(const std::filesystem::path& path,
                                        const std::filesystem::path& contextRoot);

}  // namespace chaos::orchestrator::containers::internal::detail
