#include "perturbations/CpuCapPerturbation.hpp"

#include <system_error>
#include <cstdlib>
#include <string>

namespace chaos::orchestrator::perturbations {

CpuCapPerturbation::CpuCapPerturbation(manifests::Parameters params) 
    : params_(std::move(params)) {}

void CpuCapPerturbation::apply(containers::IContainerEngine& /*engine*/, const manifests::Target& target) {
    if (target.name.empty()) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Target name is empty");
    }

    auto limit_it = params_.find("quota");
    if (limit_it == params_.end()) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Missing quota parameter");
    }

    std::string quota = limit_it->second;

    // TODO: Ideally interact with /sys/fs/cgroup natively for cpu.max
    std::string cmd = "docker update --cpu-quota " + quota + " " + target.name;
    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), "Failed to apply cpu cgroup quota via docker update");
    }
}

}  // namespace chaos::orchestrator::perturbations
