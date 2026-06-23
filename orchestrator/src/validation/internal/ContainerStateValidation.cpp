#include "ContainerStateValidation.hpp"

namespace chaos::orchestrator::validation {

ValidationResult ContainerRunningValidation::validate(const core::TargetState& target_state,
                                                      const manifests::Expectation& expectation) const {
    const bool is_running = target_state.status == containers::ContainerStatus::Running;
    return {
        .passed = is_running,
        .expectation_type = expectation.type,
        .message = is_running ? "Container is running" : "Container is NOT running (expected running)",
    };
}

ValidationResult ContainerNotRunningValidation::validate(const core::TargetState& target_state,
                                                         const manifests::Expectation& expectation) const {
    const bool is_running = target_state.status == containers::ContainerStatus::Running;
    return {
        .passed = !is_running,
        .expectation_type = expectation.type,
        .message = !is_running ? "Container is not running" : "Container IS running (expected not running)",
    };
}

}  // namespace chaos::orchestrator::validation
