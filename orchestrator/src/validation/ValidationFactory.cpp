#include "validation/ValidationFactory.hpp"

#include "internal/ContainerStateValidation.hpp"
#include "internal/HttpValidation.hpp"
#include "internal/LogValidation.hpp"

namespace chaos::orchestrator::validation {

std::unique_ptr<IValidation> createValidator(const manifests::Expectation& expectation) {
    if (expectation.type == "container_running") {
        return std::make_unique<ContainerRunningValidation>();
    } else if (expectation.type == "container_not_running") {
        return std::make_unique<ContainerNotRunningValidation>();
    } else if (expectation.type == "log_contains") {
        return std::make_unique<LogContainsValidation>();
    } else if (expectation.type == "log_not_contains") {
        return std::make_unique<LogNotContainsValidation>();
    } else if (expectation.type == "http_status") {
        return std::make_unique<HttpStatusValidation>();
    } else if (expectation.type == "http_latency") {
        return std::make_unique<HttpLatencyValidation>();
    }

    throw std::invalid_argument("Unsupported expectation type or type not yet implemented: " + expectation.type);
}

}  // namespace chaos::orchestrator::validation
