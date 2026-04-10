/// @file CpuCapPerturbation.cpp
/// @brief Implements CPU throttling via Docker Update API.

#include "perturbations/CpuCapPerturbation.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::perturbations {

namespace {
constexpr std::string_view kDefaultCpuPeriodUs = "100000";
}

CpuCapPerturbation::CpuCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                       const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

/**
 * @brief Applies the CPU cap by invoking the Docker Update API.
 *
 * The "quota" parameter should be specified in microseconds (e.g. "50000" for 50ms per 100ms period).
 */
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

    const std::string& quota = limit_it->second;
    const auto period_it = params_.find("period");
    const std::string period =
        (period_it == params_.end()) ? std::string{kDefaultCpuPeriodUs} : period_it->second;

    try {
        engine_->updateResources(target_id_, 0, std::stoll(quota), std::stoll(period));
        hasBeenApplied_ = true;
        SPDLOG_INFO("CPU Cap Perturbation applied: quota={}us/{}us on target {}", quota, period, target_id_);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply CPU cap: ") + e.what());
    }
}

/**
 * @brief Reverts the CPU cap by invoking the Docker Update API with 0 quota.
 */
void CpuCapPerturbation::revert() {
    if (!hasBeenApplied_) {
        SPDLOG_WARN("CPU Cap Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        engine_->updateResources(target_id_, 0, 0, 0);
        hasBeenApplied_ = false;
        SPDLOG_INFO("CPU Cap Perturbation reverted on target {}", target_id_);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert CPU cap: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
