#include "ContainerStateValidation.hpp"

namespace chaos::orchestrator::validation {

ValidationResult ContainerRunningValidation::validate(const shared::TargetState& targetState,
                                                      const manifests::Expectation& expectation) const {
    const bool isRunning = targetState.status == shared::ContainerStatus::Running;
    return {
        .passed = isRunning,
        .expectationType = expectation.type,
        .message = isRunning ? "Container is running" : "Container is NOT running (expected running)",
    };
}

ValidationResult ContainerNotRunningValidation::validate(const shared::TargetState& targetState,
                                                         const manifests::Expectation& expectation) const {
    const bool isRunning = targetState.status == shared::ContainerStatus::Running;
    return {
        .passed = !isRunning,
        .expectationType = expectation.type,
        .message = !isRunning ? "Container is not running" : "Container IS running (expected not running)",
    };
}

}  // namespace chaos::orchestrator::validation
