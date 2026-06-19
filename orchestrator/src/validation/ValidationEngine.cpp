#include "validation/ValidationEngine.hpp"

#include <utility>
#include <vector>

#include "validation/ValidationFactory.hpp"

namespace chaos::orchestrator::validation {

std::vector<ValidationResult> validate(const core::TargetState& targetState,
                                       const std::vector<manifests::Expectation>& expectations) {
    std::vector<ValidationResult> results;
    results.reserve(expectations.size());

    for (const auto& expectation : expectations) {
        auto validator = createValidator(expectation);
        ValidationResult result = validator->validate(targetState, expectation);

        results.push_back(std::move(result));
    }

    return results;
}

}  // namespace chaos::orchestrator::validation
