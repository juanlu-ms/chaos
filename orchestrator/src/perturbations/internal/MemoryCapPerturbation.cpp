#include "MemoryCapPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
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
        hasBeenApplied_ = false;
        throw std::invalid_argument("Target ID is empty");
    }

    auto limit_it = params_.find("limit_bytes");
    if (limit_it == params_.end()) {
        hasBeenApplied_ = false;
        throw std::invalid_argument("Missing limit_bytes parameter");
    }

    const std::string& limit = limit_it->second;

    try {
        auto sys_info = engine_->getSystemInfo();
        original_memory_limit_ = sys_info.mem_total;
    } catch (const containers::ContainerEngineError& e) {
        hasBeenApplied_ = false;
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply Memory cap: ") + e.what());
    }

    try {
        engine_->updateMemoryLimit(target_id_, std::stoll(limit));
        SPDLOG_INFO("Memory Cap Perturbation applied: limit_bytes={} on target {}", limit, target_id_);
    } catch (const containers::ContainerEngineError& e) {
        // A cap below the target's current usage kills it on the spot, and the engine may report
        // that as a failed update: with cgroup v2 and the systemd driver, the OOM killer destroys
        // the container's scope before the runtime has finished reading it back. The fault did land,
        // so the target's own state decides, not the status code.
        if (targetDiedApplyingCap()) {
            return;
        }
        hasBeenApplied_ = false;
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply Memory cap: ") + e.what());
    }
}

bool MemoryCapPerturbation::targetDiedApplyingCap() const {
    for (int attempt = 0; attempt < kStatusCheckAttempts; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(kStatusCheckDelay);
        }

        containers::ContainerStatus status = containers::ContainerStatus::Unknown;
        try {
            if (engine_->wasOomKilled(target_id_)) {
                SPDLOG_WARN(
                    "Memory Cap Perturbation: the OOM killer terminated target {} while applying the cap; "
                    "treating the cap as applied",
                    target_id_);
                return true;
            }
            status = engine_->getStatus(target_id_);
        } catch (const containers::ContainerEngineError& e) {
            SPDLOG_DEBUG("Memory Cap Perturbation: could not read state of target {}: {}", target_id_, e.what());
            return false;
        }

        if (status == containers::ContainerStatus::Exited || status == containers::ContainerStatus::Dead) {
            SPDLOG_WARN(
                "Memory Cap Perturbation: target {} died while applying the cap, with no evidence of an OOM "
                "kill; treating the cap as applied",
                target_id_);
            return true;
        }

        // Anything other than a target still running is inconclusive, and retrying would not make it
        // any clearer.
        if (status != containers::ContainerStatus::Running) {
            return false;
        }
    }

    return false;
}

void MemoryCapPerturbation::revert() {
    if (bool expected = true; !hasBeenApplied_.compare_exchange_strong(expected, false)) {
        SPDLOG_WARN("Memory Cap Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        hasBeenApplied_ = true;
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        engine_->updateMemoryLimit(target_id_, original_memory_limit_);
        SPDLOG_INFO("Memory Cap Perturbation reverted on target {}", target_id_);
    } catch (const containers::ContainerEngineError& e) {
        hasBeenApplied_ = true;
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert Memory cap: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
