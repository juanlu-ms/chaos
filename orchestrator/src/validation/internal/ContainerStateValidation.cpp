#include "validation/internal/ContainerStateValidation.hpp"

namespace chaos::orchestrator::observability {

ValidationResult ContainerRunningValidation::validate(const ValidationContext& ctx,
                                                      const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;
    result.passed = ctx.obs.isRunning(ctx.containerId);
    result.message = result.passed ? "Container is running" : "Container is NOT running (expected running)";
    return result;
}

ValidationResult ContainerNotRunningValidation::validate(const ValidationContext& ctx,
                                                         const manifests::Expectation& expectation) const {
    ValidationResult result;
    result.expectationType = expectation.type;
    result.passed = !ctx.obs.isRunning(ctx.containerId);
    result.message = result.passed ? "Container is not running" : "Container IS running (expected not running)";
    return result;
}

}  // namespace chaos::orchestrator::observability
