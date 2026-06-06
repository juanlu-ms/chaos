#include "KillPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <string>
#include <system_error>

namespace chaos::orchestrator::perturbations {

KillPerturbation::KillPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id)
    : engine_(std::move(engine)), target_id_(std::move(target_id)) {}

void KillPerturbation::apply() {
    if (bool expected = false; !hasBeenApplied_.compare_exchange_strong(expected, true)) {
        SPDLOG_WARN("Kill Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        hasBeenApplied_ = false;
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        engine_->killContainer(target_id_);
        SPDLOG_INFO("Kill Perturbation applied successfully to target {}", target_id_);
    } catch (const std::exception& e) {
        hasBeenApplied_ = false;
        throw std::system_error(std::make_error_code(std::errc::operation_canceled), e.what());
    }
}

void KillPerturbation::revert() {
    if (bool expected = true; !hasBeenApplied_.compare_exchange_strong(expected, false)) {
        SPDLOG_WARN("Kill Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        hasBeenApplied_ = true;
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        engine_->startContainer(target_id_);
        SPDLOG_INFO("Kill Perturbation reverted successfully for target {}", target_id_);
    } catch (const std::exception& e) {
        hasBeenApplied_ = true;
        throw std::system_error(std::make_error_code(std::errc::operation_canceled), e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
