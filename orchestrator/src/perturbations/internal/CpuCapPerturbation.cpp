#include "CpuCapPerturbation.hpp"

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

void CpuCapPerturbation::apply() {
    if (bool expected = false; !hasBeenApplied_.compare_exchange_strong(expected, true)) {
        SPDLOG_WARN("CPU Cap Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        hasBeenApplied_ = false;
        throw std::invalid_argument("Target ID is empty");
    }

    auto limit_it = params_.find("cpu_cores");
    if (limit_it == params_.end()) {
        hasBeenApplied_ = false;
        throw std::invalid_argument("Missing cpu_cores parameter");
    }

    double cpu_limit = 0.0;
    try {
        cpu_limit = std::stod(limit_it->second);
    } catch (const std::exception&) {
        hasBeenApplied_ = false;
        throw std::invalid_argument("cpu_cores must be a valid number");
    }

    if (cpu_limit <= 0.0) {
        hasBeenApplied_ = false;
        throw std::invalid_argument("cpu_cores must be greater than 0");
    }

    auto cpu_quota = static_cast<int64_t>(cpu_limit * kCpuPeriod);

    try {
        engine_->updateCpuQuota(target_id_, cpu_quota, kCpuPeriod);
        SPDLOG_INFO("CPU Cap Perturbation applied: quota={}us/{}us on target {}", cpu_quota, kCpuPeriod, target_id_);
    } catch (const containers::ContainerEngineError& e) {
        hasBeenApplied_ = false;
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply CPU cap: ") + e.what());
    }
}

void CpuCapPerturbation::revert() {
    if (bool expected = true; !hasBeenApplied_.compare_exchange_strong(expected, false)) {
        SPDLOG_WARN("CPU Cap Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        hasBeenApplied_ = true;
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        SPDLOG_INFO("Reverting CPU cap for target '{}'", target_id_);
        engine_->updateCpuQuota(target_id_, kDefaultCpuQuota, kCpuPeriod);
        SPDLOG_INFO("CPU Cap Perturbation reverted on target {}", target_id_);
    } catch (const containers::ContainerEngineError& e) {
        hasBeenApplied_ = true;
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert CPU cap: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
