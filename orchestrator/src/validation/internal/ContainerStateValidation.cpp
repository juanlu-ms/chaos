#include "ContainerStateValidation.hpp"

namespace chaos::orchestrator::validation {

ValidationResult ContainerRunningValidation::validate(const shared::TargetState& targetState,
                                                      const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;
    result.passed = targetState.status == shared::ContainerStatus::Running;
    result.message = result.passed ? "Container is running" : "Container is NOT running (expected running)";
    return result;
}

ValidationResult ContainerNotRunningValidation::validate(const shared::TargetState& targetState,
                                                         const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;
    result.passed = targetState.status != shared::ContainerStatus::Running;
    result.message = result.passed ? "Container is not running" : "Container IS running (expected not running)";
    return result;
}

}  // namespace chaos::orchestrator::validation
