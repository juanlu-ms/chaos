/**
 * @file CgroupMetricsGatherer.hpp
 * @brief Reads CPU and memory metrics directly from cgroup v2 files,
 *        bypassing the Docker API for sub-millisecond latency.
 */

#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace chaos::orchestrator::containers::internal {

/**
 * @brief Gathers CPU and memory usage from the host's cgroup v2 hierarchy.
 *
 * Each container's cgroup lives under
 *   /sys/fs/cgroup/system.slice/docker-<id>.scope/
 *
 * CPU:   cpu.stat -> { usage_usec  (total CPU time in microseconds) }
 * Memory: memory.current (total current memory in bytes)
 *
 * If cgroup files are not accessible (permissions, non-Linux, cgroups v1)
 * the class reports no value and the caller should fall back to the
 * Docker stats API.
 */
class CgroupMetricsGatherer {
public:
    /**
     * @brief Try to locate the cgroup v2 directory for a Docker container.
     * @return Path like /sys/fs/cgroup/system.slice/docker-<id>.scope/,
     *         or empty if not found.
     */
    static std::optional<std::filesystem::path>
    resolveCgroupPath(const std::string_view containerId);

    /**
     * @brief Read CPU usage percent since the last call for this instance.
     *
     * Reads cpu.stat, computes the delta in total usage_usec, and divides
     * by the wall-clock delta since the previous read.  Returns 0 on the
     * first call (no delta to compute).
     *
     * @return CPU usage in percent (0.0 – 100.0 * nr_cpus), or nullopt.
     */
    std::optional<double> getCpuUsagePercent(const std::string_view containerId);

    /**
     * @brief Read current memory usage in MB.
     * @return Memory in MB, or nullopt.
     */
    std::optional<double> getMemoryUsageMb(const std::string_view containerId);

private:
    // Last CPU usage value for delta computation.
    uint64_t prev_cpu_usec_ = 0;
    std::chrono::steady_clock::time_point prev_cpu_time_;
    bool prev_valid_ = false;
};

}  // namespace chaos::orchestrator::containers::internal
