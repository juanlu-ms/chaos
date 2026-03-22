#include "perturbations/NetworkDelayPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <system_error>

namespace chaos::orchestrator::perturbations {

NetworkDelayPerturbation::NetworkDelayPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                                   std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

void NetworkDelayPerturbation::apply() {
    if (hasBeenApplied_) {
        SPDLOG_WARN("Network Delay Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    auto delay_it = params_.find("delay_ms");
    if (delay_it == params_.end()) {
        throw std::invalid_argument("Missing delay_ms parameter for network cap");
    }

    std::string delay = delay_it->second;

    std::string cmd = "docker exec " + target_id_ + " tc qdisc add dev eth0 root netem delay " + delay + "ms";
    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                "Failed to apply tc netem delay inside container");
    }
}

void NetworkDelayPerturbation::revert() {
    if (!hasBeenApplied_) {
        SPDLOG_WARN("Network Delay Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    std::string cmd = "docker exec " + target_id_ + " tc qdisc del dev eth0 root netem";
    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                "Failed to revert tc netem delay inside container");
    }
}

}  // namespace chaos::orchestrator::perturbations
