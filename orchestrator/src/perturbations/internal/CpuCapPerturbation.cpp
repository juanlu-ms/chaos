#include "perturbations/CpuCapPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::perturbations {

CpuCapPerturbation::CpuCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                       const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

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

    std::string quota = limit_it->second;

    // TODO: Implement cpu limit with cgroups
    throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                            "Failed to apply cpu cgroup quota");
}

void CpuCapPerturbation::revert() {
    if (!hasBeenApplied_) {
        SPDLOG_WARN("CPU Cap Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    // TODO: Implement cpu limit revert with cgroups
}

}  // namespace chaos::orchestrator::perturbations
