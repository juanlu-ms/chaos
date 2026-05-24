/**
 * @file CgroupMetricsInternal.hpp
 * @brief Low-level cgroup v2 file reading helpers extracted from readU64.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace chaos::orchestrator::containers::internal::detail {

// Read a uint64_t from a cgroup file containing a single integer value.
// Returns the integer value, or nullopt if the file cannot be read or parsed.
[[nodiscard]] std::optional<uint64_t> readSimpleU64(const std::filesystem::path& path);

// Read a uint64_t by key from a multi-field cgroup file (e.g. cpu.stat).
// Returns the integer value for the matching key, or nullopt if not found.
[[nodiscard]] std::optional<uint64_t> readKeyValueU64(const std::filesystem::path& path, std::string_view key);

}  // namespace chaos::orchestrator::containers::internal::detail
