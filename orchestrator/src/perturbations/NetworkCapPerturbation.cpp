#include "perturbations/NetworkCapPerturbation.hpp"

#include <system_error>
#include <cstdlib>
#include <string>

namespace chaos::orchestrator::perturbations {

NetworkCapPerturbation::NetworkCapPerturbation(manifests::Parameters params) 
    : params_(std::move(params)) {}

void NetworkCapPerturbation::apply(containers::IContainerEngine& /*engine*/, const manifests::Target& target) {
    if (target.name.empty()) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Target name is empty");
    }

    auto delay_it = params_.find("delay_ms");
    if (delay_it == params_.end()) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Missing delay_ms parameter for network cap");
    }

    std::string delay = delay_it->second;

    // Use tc qdisc internally. We can use `docker exec` to run it in the container's namespace
    std::string cmd = "docker exec " + target.name + " tc qdisc add dev eth0 root netem delay " + delay + "ms";
    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), "Failed to apply tc netem delay inside container");
    }
}

}  // namespace chaos::orchestrator::perturbations
