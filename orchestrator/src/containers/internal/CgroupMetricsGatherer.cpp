/**
 * @file CgroupMetricsGatherer.cpp
 * @brief Implementation of cgroup v2 metric collection.
 */

#include "CgroupMetricsGatherer.hpp"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <fstream>
#include <string>

#include "CgroupMetricsInternal.hpp"

namespace chaos::orchestrator::containers::internal::detail {

std::optional<uint64_t> readSimpleU64(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::string line;
    if (!std::getline(file, line)) {
        return std::nullopt;
    }
    try {
        size_t pos = 0;
        uint64_t val = std::stoull(line, &pos);
        if (pos == line.size()) {
            return val;
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<uint64_t> readKeyValueU64(const std::filesystem::path& path, std::string_view key) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::string line;
    std::string currentKey;
    uint64_t val = 0;
    while (file >> currentKey >> val) {
        if (currentKey == key) {
            return val;
        }
    }
    return std::nullopt;
}

}  // namespace chaos::orchestrator::containers::internal::detail

namespace chaos::orchestrator::containers::internal {

namespace {

constexpr auto CGROUP_ROOT = "/sys/fs/cgroup/system.slice";

}  // namespace

std::optional<std::filesystem::path> CgroupMetricsGatherer::resolveCgroupPath(const std::string_view containerId) {
    namespace fs = std::filesystem;
    const fs::path base(CGROUP_ROOT);
    std::error_code ec;
    if (!fs::is_directory(base, ec)) {
        return std::nullopt;
    }

    const std::string idStr(containerId);
    for (const auto& entry : fs::directory_iterator(base, ec)) {
        if (!entry.is_directory(ec)) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.contains(idStr)) {
            return entry.path();
        }
    }
    return std::nullopt;
}

std::optional<double> CgroupMetricsGatherer::getCpuUsagePercent(const std::string_view containerId) {
    auto dir = resolveCgroupPath(containerId);
    if (!dir) {
        return std::nullopt;
    }

    auto path = *dir / "cpu.stat";
    auto current = detail::readKeyValueU64(path, "usage_usec");
    if (!current.has_value()) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    double percent = 0.0;
    if (prev_valid_) {
        auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - prev_cpu_time_).count();
        if (dt > 0.0) {
            percent = static_cast<double>(*current - prev_cpu_usec_) / (dt * 10000.0);
        }
    }
    prev_cpu_usec_ = *current;
    prev_cpu_time_ = now;
    prev_valid_ = true;
    return percent;
}

std::optional<double> CgroupMetricsGatherer::getMemoryUsageMb(const std::string_view containerId) {
    auto dir = resolveCgroupPath(containerId);
    if (!dir) {
        return std::nullopt;
    }

    auto path = *dir / "memory.current";
    auto bytes = detail::readSimpleU64(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }

    return static_cast<double>(*bytes) / (1024.0 * 1024.0);
}

}  // namespace chaos::orchestrator::containers::internal
