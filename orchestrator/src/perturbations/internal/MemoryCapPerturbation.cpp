/// @file MemoryCapPerturbation.cpp
/// @brief Implements memory limiting via cgroups v2 memory.max controller.

#include "perturbations/MemoryCapPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "PerturbationUtils.hpp"

namespace chaos::orchestrator::perturbations {

namespace {

std::string resolveCgroupPath(const std::string& pid) {
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
}

}  // namespace

MemoryCapPerturbation::MemoryCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                             std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

/**
 * @brief Applies the memory cap by writing to the container's cgroup v2 memory.max file.
 *
 * The "limit_bytes" parameter should be specified in bytes (e.g. "104857600" for 100MB).
 * Resolves the cgroup path dynamically using the container's PID from /proc.
 *
 * @throws std::invalid_argument If target ID is missing.
 * @throws std::system_error If limit_bytes parameter is missing or write fails.
 */
void MemoryCapPerturbation::apply() {
    if (hasBeenApplied_) {
        SPDLOG_WARN("Memory Cap Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    auto limit_it = params_.find("limit_bytes");
    if (limit_it == params_.end()) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Missing limit_bytes parameter");
    }

    const std::string& limit = limit_it->second;

    try {
        const std::string pid = internal::fetchContainerPid(target_id_);
        const std::string memMaxPath = resolveCgroupPath(pid) + "/memory.max";

        internal::writeCgroupFile(memMaxPath, limit);

        hasBeenApplied_ = true;
        SPDLOG_INFO("Memory Cap Perturbation applied: limit_bytes={} on target {}", limit, target_id_);
    } catch (const std::system_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), e.what());
    }
}

/**
 * @brief Reverts the memory cap by writing "max" to the memory.max cgroup file.
 *
 * "max" restores the default unlimited memory access.
 *
 * @throws std::system_error On failure to revert the cgroup file.
 */
void MemoryCapPerturbation::revert() {
    if (!hasBeenApplied_) {
        SPDLOG_WARN("Memory Cap Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        const std::string pid = internal::fetchContainerPid(target_id_);
        const std::string memMaxPath = resolveCgroupPath(pid) + "/memory.max";

        internal::writeCgroupFile(memMaxPath, "max");

        hasBeenApplied_ = false;
        SPDLOG_INFO("Memory Cap Perturbation reverted on target {}", target_id_);
    } catch (const std::system_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
