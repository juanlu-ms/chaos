/// @file PerturbationUtils.hpp
/// @brief Shared internal utilities for perturbation implementations.

#pragma once

#include <string>

namespace chaos::orchestrator::perturbations::internal {

/**
 * @brief Fetches the host-level PID of a container using 'docker inspect'.
 *
 * Uses RAII (unique_ptr with pclose deleter) and std::array for the read
 * buffer — no raw FILE* leaks or C-style char arrays.
 *
 * @param containerId Docker container ID or name.
 * @return PID as a string.
 * @throws std::runtime_error On failure or if PID resolves to "0".
 */
std::string fetchContainerPid(const std::string& containerId);

/**
 * @brief Runs a shell command via popen and throws on non-zero exit.
 *
 * Captures stderr/stdout output via popen (RAII). On failure, the captured
 * output is included in the exception message for easier debugging.
 *
 * @param cmd Shell command to execute.
 * @param errorMsg Prefix added to the exception message on failure.
 * @throws std::system_error If the command exits with non-zero status.
 */
void runCommand(const std::string& cmd, const std::string& errorMsg);

/**
 * @brief Writes a value to a cgroup controller file.
 *
 * @param path Absolute path to the cgroup file (e.g. /sys/fs/cgroup/.../cpu.max).
 * @param value Value to write.
 * @throws std::system_error On open or write failure.
 */
void writeCgroupFile(const std::string& path, const std::string& value);

}  // namespace chaos::orchestrator::perturbations::internal
