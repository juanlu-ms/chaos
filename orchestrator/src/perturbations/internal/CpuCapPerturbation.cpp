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
constexpr int64_t kCpuPeriod = 100000;
constexpr int64_t kDefaultCpuQuota = -1;
}  // namespace

CpuCapPerturbation::CpuCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine, std::string target_id,
                                       const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

/**
 * @brief Applies the CPU cap by invoking the Docker Update API.
 *
 * The "cpu_cores" parameter represents the maximum number of CPU cores allowed
 * for the target container and supports decimal values (e.g. "0.5", "1.25", "2.0").
 */
void CpuCapPerturbation::apply() {
    if (hasBeenApplied_) {
        SPDLOG_WARN("CPU Cap Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    auto limit_it = params_.find("cpu_cores");
    if (limit_it == params_.end()) {
        throw std::invalid_argument("Missing cpu_cores parameter");
    }

    double cpu_limit = 0.0;
    try {
        cpu_limit = std::stod(limit_it->second);
    } catch (const std::exception&) {
        throw std::invalid_argument("cpu_cores must be a valid number");
    }

    if (cpu_limit <= 0.0) {
        throw std::invalid_argument("cpu_cores must be greater than 0");
    }

    auto cpu_quota = static_cast<int64_t>(cpu_limit * kCpuPeriod);

    try {
        engine_->updateCpuQuota(target_id_, cpu_quota, kCpuPeriod);
        hasBeenApplied_ = true;
        SPDLOG_INFO("CPU Cap Perturbation applied: quota={}us/{}us on target {}", cpu_quota, kCpuPeriod, target_id_);
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
        SPDLOG_INFO("Reverting CPU cap for target '{}'", target_id_);
        engine_->updateCpuQuota(target_id_, kDefaultCpuQuota, kCpuPeriod);
        hasBeenApplied_ = false;
        SPDLOG_INFO("CPU Cap Perturbation reverted on target {}", target_id_);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert CPU cap: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
