#include "perturbations/MemoryCapPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <system_error>

namespace chaos::orchestrator::perturbations {

MemoryCapPerturbation::MemoryCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                             std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

/**
 * @brief Applies the memory cap by modifying the container's cgroup.
 * @throws std::system_error On failure to cap memory.
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

    std::string limit = limit_it->second;

    // TODO: Implement memory limit with cgroups
    throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                            "Failed to apply memory cgroup limit via docker update");
}

/**
 * @brief Reverts the memory cap by removing the cgroup restriction.
 * @throws std::system_error On failure to revert memory cap.
 */
void MemoryCapPerturbation::revert() {
    if (!hasBeenApplied_) {
        SPDLOG_WARN("Memory Cap Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    // TODO: Implement memory cap revert logic
    throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                            "Failed to revert memory cgroup limit via docker update");
}

}  // namespace chaos::orchestrator::perturbations
