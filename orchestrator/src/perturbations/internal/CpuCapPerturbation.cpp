#include "perturbations/CpuCapPerturbation.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace chaos::orchestrator::perturbations {

CpuCapPerturbation::CpuCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                       const manifests::Perturbation& spec)
    : engine_(std::move(engine)), params_(spec.parameters) {}

void CpuCapPerturbation::apply(const manifests::Target& target) {
    if (target.name.empty()) {
        throw std::invalid_argument("Target name is empty");
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

void CpuCapPerturbation::revert(const manifests::Target& target) {
    // TODO: Implement cpu limit revert with cgroups
    if (target.name.empty()) {
        throw std::invalid_argument("Target name is empty");
    }
}

}  // namespace chaos::orchestrator::perturbations
