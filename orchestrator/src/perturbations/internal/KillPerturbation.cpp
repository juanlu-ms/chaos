#include "perturbations/KillPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <system_error>

namespace chaos::orchestrator::perturbations {

KillPerturbation::KillPerturbation(std::shared_ptr<containers::IContainerEngine> engine) : engine_(std::move(engine)) {}

void KillPerturbation::apply(const manifests::Target& target) {
    if (target.name.empty()) {
        throw std::invalid_argument("Target name is empty");
    }

    try {
        engine_->killContainer(target.name);
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_canceled), e.what());
    }
}

void KillPerturbation::revert(const manifests::Target& target) {
    if (target.name.empty()) {
        throw std::invalid_argument("Target name is empty");
    }

    try {
        engine_->startContainer(target.name);
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_canceled), e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
