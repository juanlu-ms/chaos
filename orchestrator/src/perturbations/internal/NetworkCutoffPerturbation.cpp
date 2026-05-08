#include "NetworkCutoffPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::perturbations {

NetworkCutoffPerturbation::NetworkCutoffPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                                     std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

void NetworkCutoffPerturbation::apply() {
    if (hasBeenApplied_) {
        SPDLOG_WARN("Network Cutoff Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        bool hasFilter = false;

        auto addRule = [&](const std::string& applyArgs, const std::string& revertArgs) {
            (void)engine_->exec(target_id_, "iptables " + applyArgs);
            revertCommands_.push_back("iptables " + revertArgs);
            hasFilter = true;
        };

        auto it = params_.find("dst_ip");
        if (it != params_.end()) {
            addRule("-A OUTPUT -d " + it->second + " -j DROP", "-D OUTPUT -d " + it->second + " -j DROP");
        }

        it = params_.find("dst_port");
        if (it != params_.end()) {
            addRule("-A OUTPUT -p tcp --dport " + it->second + " -j DROP",
                    "-D OUTPUT -p tcp --dport " + it->second + " -j DROP");
        }

        it = params_.find("src_port");
        if (it != params_.end()) {
            addRule("-A INPUT -p tcp --dport " + it->second + " -j DROP",
                    "-D INPUT -p tcp --dport " + it->second + " -j DROP");
        }

        if (!hasFilter) {
            // No filters: block all egress and ingress
            addRule("-A OUTPUT -j DROP", "-D OUTPUT -j DROP");
            addRule("-A INPUT -j DROP", "-D INPUT -j DROP");
        }

        hasBeenApplied_ = true;
        SPDLOG_INFO("Network Cutoff Perturbation applied on target {}", target_id_);

    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply iptables rules inside container: ") + e.what());
    }
}

void NetworkCutoffPerturbation::revert() {
    if (!hasBeenApplied_) {
        SPDLOG_WARN("Network Cutoff Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        for (const auto& cmd : revertCommands_) {
            (void)engine_->exec(target_id_, cmd);
        }

        revertCommands_.clear();
        hasBeenApplied_ = false;
        SPDLOG_INFO("Network Cutoff Perturbation reverted on target {}", target_id_);

    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert iptables rules inside container: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
