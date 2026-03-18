#include "perturbations/MemoryCapPerturbation.hpp"

#include <system_error>
#include <cstdlib>
#include <string>

namespace chaos::orchestrator::perturbations {

MemoryCapPerturbation::MemoryCapPerturbation(manifests::Parameters params) 
    : params_(std::move(params)) {}

void MemoryCapPerturbation::apply(containers::IContainerEngine& /*engine*/, const manifests::Target& target) {
    if (target.name.empty()) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Target name is empty");
    }

    auto limit_it = params_.find("limit_bytes");
    if (limit_it == params_.end()) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Missing limit_bytes parameter");
    }

    std::string limit = limit_it->second;

    // TODO: Ideally interact with /sys/fs/cgroup/system.slice/docker-<id>.scope/memory.max natively
    // For now we simulate the sys call or use docker update
    std::string cmd = "docker update --memory " + limit + " " + target.name;
    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), "Failed to apply memory cgroup limit via docker update");
    }
}

}  // namespace chaos::orchestrator::perturbations
