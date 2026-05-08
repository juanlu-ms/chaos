/**
 * @file CgroupMetricsGatherer.cpp
 * @brief Implementation of cgroup v2 metric collection.
 */

#include "CgroupMetricsGatherer.hpp"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <fstream>
#include <string>

namespace chaos::orchestrator::containers::internal {

namespace {

constexpr auto CGROUP_ROOT = "/sys/fs/cgroup/system.slice";

// Read the first uint64_t from a single-key cgroup v2 file.
std::optional<uint64_t> readU64(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return std::nullopt;
    // cpu.stat has multiple key-value pairs; memory.current has a single number.
    // Handle both: try number first, then "key value" line.
    std::string line;
    if (!std::getline(f, line)) return std::nullopt;
    // Try single number
    try {
        size_t pos = 0;
        uint64_t val = std::stoull(line, &pos);
        if (pos == line.size()) return val;
    } catch (...) {}
    // Key-value format (cpu.stat): find the usage_usec line
    std::istringstream iss(line);
    // Actually re-read the whole file for cpu.stat which has multiple lines
    f.clear();
    f.seekg(0);
    std::string key;
    uint64_t val;
    // The key is "usage_usec" for cpu.stat
    while (f >> key >> val) {
        if (key == "usage_usec") return val;
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::filesystem::path>
CgroupMetricsGatherer::resolveCgroupPath(const std::string_view containerId) {
    namespace fs = std::filesystem;
    const fs::path base(CGROUP_ROOT);
    std::error_code ec;
    if (!fs::is_directory(base, ec)) return std::nullopt;

    // Look for a directory whose name contains the container ID.
    // With systemd + cgroups v2 the name is docker-<full-id>.scope.
    const std::string idStr(containerId);
    for (const auto& entry : fs::directory_iterator(base, ec)) {
        if (!entry.is_directory(ec)) continue;
        const auto name = entry.path().filename().string();
        if (name.find(idStr) != std::string::npos) {
            return entry.path();
        }
    }
    return std::nullopt;
}

std::optional<double> CgroupMetricsGatherer::getCpuUsagePercent(const std::string_view containerId) {
    auto dir = resolveCgroupPath(containerId);
    if (!dir) return std::nullopt;

    auto path = *dir / "cpu.stat";
    auto current = readU64(path);
    if (!current) return std::nullopt;

    auto now = std::chrono::steady_clock::now();
    double percent = 0.0;
    if (prev_valid_) {
        auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - prev_cpu_time_).count();
        if (dt > 0.0) {
            // usage_usec is in microseconds, dt is in seconds.
            // usage delta (us) / dt (s) = usage per second in us.
            // To get percent, divide by 10000 (since 1 core for 1s = 1e6 us).
            // For n cores, the total can exceed 100%.
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
    if (!dir) return std::nullopt;

    auto path = *dir / "memory.current";
    auto bytes = readU64(path);
    if (!bytes) return std::nullopt;

    return static_cast<double>(*bytes) / (1024.0 * 1024.0);
}

}  // namespace chaos::orchestrator::containers::internal
