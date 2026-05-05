/// @file NetworkDelayPerturbation.cpp
/// @brief Implements network delay via tc netem inside the container using IContainerEngine::exec.

#include "perturbations/NetworkDelayPerturbation.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "containers/IContainerEngine.hpp"

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
        throw std::invalid_argument("Missing delay_ms parameter for network delay perturbation");
    }

    const std::string& delay = delay_it->second;

    try {
        engine_->exec(target_id_, fmt::format("tc qdisc add dev eth0 root netem delay {}ms", delay));
        hasBeenApplied_ = true;
        SPDLOG_INFO("Network Delay Perturbation applied: delay={}ms on target {}", delay, target_id_);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply tc netem delay inside container: ") + e.what());
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

    try {
        engine_->exec(target_id_, "tc qdisc del dev eth0 root netem");
        hasBeenApplied_ = false;
        SPDLOG_INFO("Network Delay Perturbation reverted on target {}", target_id_);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert tc netem delay inside container: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
