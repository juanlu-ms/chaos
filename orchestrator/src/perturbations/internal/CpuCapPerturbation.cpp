/// @file CpuCapPerturbation.cpp
/// @brief Implements CPU throttling via cgroups v2 cpu.max controller.

#include "perturbations/CpuCapPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::perturbations {

namespace {

/**
 * @brief Resolves the cgroup v2 path for a container via its PID.
 *
 * Reads /proc/<pid>/cgroup and extracts the hierarchy path.
 * Then maps that to /sys/fs/cgroup/ to produce the full controller path.
 *
 * @param pid Host-level PID of the container's init process.
 * @return Absolute path to the container's cgroup v2 directory.
 * @throws std::runtime_error If the cgroup file cannot be parsed.
 */
std::string resolveCgroupPath(const std::string& pid) {
    std::ifstream cgroupFile("/proc/" + pid + "/cgroup");
    if (!cgroupFile.is_open()) {
        throw std::runtime_error("Failed to open /proc/" + pid + "/cgroup");
    }

    std::string line;
    while (std::getline(cgroupFile, line)) {
        // cgroups v2 entries are prefixed with "0::"
        if (line.rfind("0::", 0) == 0) {
            std::string relativePath = line.substr(3);  // strip "0::"
            return "/sys/fs/cgroup" + relativePath;
        }
    }

    throw std::runtime_error("Unable to find cgroups v2 entry in /proc/" + pid + "/cgroup");
}

/**
 * @brief Fetches the host-level PID of a container using 'docker inspect'.
 *
 * @param containerId Docker container ID or name.
 * @return PID as a string.
 * @throws std::runtime_error On failure.
 */
std::string fetchContainerPid(const std::string& containerId) {
    std::string cmd = "docker inspect --format '{{.State.Pid}}' " + containerId;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to run docker inspect to fetch PID");
    }

    char buffer[64];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    // Trim whitespace
    while (!result.empty() && (result.back() == '\n' || result.back() == ' ')) {
        result.pop_back();
    }

    if (result.empty() || result == "0") {
        throw std::runtime_error("Failed to resolve container PID for: " + containerId);
    }

    return result;
}

/**
 * @brief Writes a value to a cgroup controller file.
 * @param path Absolute path to the cgroup file.
 * @param value Value to write.
 * @throws std::system_error On write failure.
 */
void writeCgroupFile(const std::string& path, const std::string& value) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::system_error(std::make_error_code(std::errc::permission_denied),
                                "Failed to open cgroup file: " + path);
    }
    file << value;
    if (!file) {
        throw std::system_error(std::make_error_code(std::errc::io_error),
                                "Failed to write to cgroup file: " + path);
    }
}

}  // namespace

CpuCapPerturbation::CpuCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                       const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

/**
 * @brief Applies the CPU cap by writing to the container's cgroup v2 cpu.max file.
 *
 * The "quota" parameter should be specified in microseconds (e.g. "50000" for 50ms per 100ms period).
 * Resolves the cgroup path dynamically using the container's PID from /proc.
 *
 * @throws std::invalid_argument If target ID or quota parameter is missing.
 * @throws std::system_error On failure to write to the cgroup file.
 */
void CpuCapPerturbation::apply() {
    if (hasBeenApplied_) {
        SPDLOG_WARN("CPU Cap Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    auto limit_it = params_.find("quota");
    if (limit_it == params_.end()) {
        throw std::invalid_argument("Missing quota parameter");
    }

    const std::string& quota = limit_it->second;

    try {
        const std::string pid = fetchContainerPid(target_id_);
        const std::string cgroupDir = resolveCgroupPath(pid);
        const std::string cpuMaxPath = cgroupDir + "/cpu.max";

        // cgroups v2 cpu.max format: "<quota> <period>" (both in microseconds)
        // Default period is 100000. Write "<quota> 100000"
        writeCgroupFile(cpuMaxPath, quota + " 100000");

        hasBeenApplied_ = true;
        SPDLOG_INFO("CPU Cap Perturbation applied: quota={}us/100000us on target {}", quota, target_id_);
    } catch (const std::system_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), e.what());
    }
}

/**
 * @brief Reverts the CPU cap by writing "max" to the cpu.max cgroup file.
 *
 * "max" restores the default unlimited CPU scheduling.
 *
 * @throws std::system_error On failure to revert the cgroup file.
 */
void CpuCapPerturbation::revert() {
    if (!hasBeenApplied_) {
        SPDLOG_WARN("CPU Cap Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        const std::string pid = fetchContainerPid(target_id_);
        const std::string cgroupDir = resolveCgroupPath(pid);
        const std::string cpuMaxPath = cgroupDir + "/cpu.max";

        writeCgroupFile(cpuMaxPath, "max 100000");

        hasBeenApplied_ = false;
        SPDLOG_INFO("CPU Cap Perturbation reverted on target {}", target_id_);
    } catch (const std::system_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
