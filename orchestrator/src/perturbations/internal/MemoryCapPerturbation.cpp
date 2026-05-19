#include "MemoryCapPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace chaos::orchestrator::perturbations {

MemoryCapPerturbation::MemoryCapPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                             std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

void MemoryCapPerturbation::apply() {
    if (bool expected = false; !hasBeenApplied_.compare_exchange_strong(expected, true)) {
        SPDLOG_WARN("Memory Cap Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    auto limit_it = params_.find("limit_bytes");
    if (limit_it == params_.end()) {
        throw std::invalid_argument("Missing limit_bytes parameter");
    }

    const std::string& limit = limit_it->second;

    try {
        engine_->updateMemoryLimit(target_id_, std::stoll(limit));
        SPDLOG_INFO("Memory Cap Perturbation applied: limit_bytes={} on target {}", limit, target_id_);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply Memory cap: ") + e.what());
    }
}

void MemoryCapPerturbation::revert() {
    if (bool expected = true; !hasBeenApplied_.compare_exchange_strong(expected, false)) {
        SPDLOG_WARN("Memory Cap Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        auto sysInfo = engine_->getSystemInfo();
        engine_->updateMemoryLimit(target_id_, sysInfo.memTotal);
        SPDLOG_INFO("Memory Cap Perturbation reverted on target {}", target_id_);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert Memory cap: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
