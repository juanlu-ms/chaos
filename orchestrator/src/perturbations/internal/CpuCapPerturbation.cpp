/// @file CpuCapPerturbation.cpp
/// @brief Implements CPU throttling via cgroups v2 cpu.max controller.

#include "perturbations/CpuCapPerturbation.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "PerturbationUtils.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::perturbations {

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
        const std::string pid = internal::fetchContainerPid(target_id_);
        const std::string cgroupDir = [&pid] {
            std::ifstream cgroupFile("/proc/" + pid + "/cgroup");
            if (!cgroupFile.is_open()) {
                throw std::runtime_error("Failed to open /proc/" + pid + "/cgroup");
            }
            std::string line;
            while (std::getline(cgroupFile, line)) {
                if (line.rfind("0::", 0) == 0) {
                    return "/sys/fs/cgroup" + line.substr(3);
                }
            }
            throw std::runtime_error("Unable to find cgroups v2 entry in /proc/" + pid + "/cgroup");
        }();

        const std::string cpuMaxPath = fmt::format("{}/cpu.max", cgroupDir);

        // cgroups v2 cpu.max format: "<quota> <period>" (both in microseconds)
        internal::writeCgroupFile(cpuMaxPath, quota + " 100000");

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
        const std::string pid = internal::fetchContainerPid(target_id_);
        const std::string cgroupDir = [&pid] {
            std::ifstream cgroupFile("/proc/" + pid + "/cgroup");
            if (!cgroupFile.is_open()) {
                throw std::runtime_error("Failed to open /proc/" + pid + "/cgroup");
            }
            std::string line;
            while (std::getline(cgroupFile, line)) {
                if (line.rfind("0::", 0) == 0) {
                    return "/sys/fs/cgroup" + line.substr(3);
                }
            }
            throw std::runtime_error("Unable to find cgroups v2 entry in /proc/" + pid + "/cgroup");
        }();

        const std::string cpuMaxPath = fmt::format("{}/cpu.max", cgroupDir);
        internal::writeCgroupFile(cpuMaxPath, "max 100000");

        hasBeenApplied_ = false;
        SPDLOG_INFO("CPU Cap Perturbation reverted on target {}", target_id_);
    } catch (const std::system_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
