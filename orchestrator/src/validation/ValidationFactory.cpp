#include "observability/ValidationFactory.hpp"

#include <stdexcept>

#include "validation/internal/ContainerStateValidation.hpp"
#include "validation/internal/HttpValidation.hpp"
#include "validation/internal/LogValidation.hpp"

namespace chaos::orchestrator::observability {

std::unique_ptr<IValidation> ValidationFactory::create(const manifests::Expectation& expectation) {
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

}  // namespace chaos::orchestrator::observability
