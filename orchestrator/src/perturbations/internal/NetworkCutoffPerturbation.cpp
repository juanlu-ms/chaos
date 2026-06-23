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
    if (bool expected = false; !hasBeenApplied_.compare_exchange_strong(expected, true)) {
        SPDLOG_WARN("Network Cutoff Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        hasBeenApplied_ = false;
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        bool has_filter = false;

        auto addRule = [&](const std::string& apply_args, const std::string& revert_args) {
            {
                const auto exec_out = engine_->execInNetNs(target_id_, "iptables " + apply_args);
                if (!exec_out.empty()) {
                    SPDLOG_TRACE("iptables output: {}", exec_out);
                }
            }
            revertCommands_.push_back("iptables " + revert_args);
            has_filter = true;
        };

        auto iter = params_.find("dst_ip");
        if (iter != params_.end()) {
            addRule("-A OUTPUT -d " + iter->second + " -j DROP", "-D OUTPUT -d " + iter->second + " -j DROP");
        }

        iter = params_.find("dst_port");
        if (iter != params_.end()) {
            addRule("-A OUTPUT -p tcp --dport " + iter->second + " -j DROP",
                    "-D OUTPUT -p tcp --dport " + iter->second + " -j DROP");
        }

        iter = params_.find("src_port");
        if (iter != params_.end()) {
            addRule("-A INPUT -p tcp --sport " + iter->second + " -j DROP",
                    "-D INPUT -p tcp --sport " + iter->second + " -j DROP");
        }

        if (!has_filter) {
            addRule("-A OUTPUT -j DROP", "-D OUTPUT -j DROP");
            addRule("-A INPUT -j DROP", "-D INPUT -j DROP");
        }

        SPDLOG_INFO("Network Cutoff Perturbation applied on target {}", target_id_);

    } catch (const containers::ContainerEngineError& e) {
        hasBeenApplied_ = false;
        for (const auto& cmd : revertCommands_) {
            try {
                const auto exec_out = engine_->execInNetNs(target_id_, cmd);
                if (!exec_out.empty()) {
                    SPDLOG_TRACE("iptables output: {}", exec_out);
                }
            } catch (...) {
                SPDLOG_ERROR("Failed to roll back iptables rule '{}' after apply failure", cmd);
            }
        }
        revertCommands_.clear();
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply iptables rules inside container: ") + e.what());
    }
}

void NetworkCutoffPerturbation::revert() {
    if (bool expected = true; !hasBeenApplied_.compare_exchange_strong(expected, false)) {
        SPDLOG_WARN("Network Cutoff Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        hasBeenApplied_ = true;
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        for (const auto& command : revertCommands_) {
            const auto exec_out = engine_->execInNetNs(target_id_, command);
            if (!exec_out.empty()) {
                SPDLOG_TRACE("iptables output: {}", exec_out);
            }
        }

        revertCommands_.clear();
        SPDLOG_INFO("Network Cutoff Perturbation reverted on target {}", target_id_);

    } catch (const containers::ContainerEngineError& e) {
        hasBeenApplied_ = true;
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert iptables rules inside container: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
