#include "perturbations/KillPerturbation.hpp"

#include <system_error>

namespace chaos::orchestrator::perturbations {

void KillPerturbation::apply(containers::IContainerEngine& engine, const manifests::Target& target) {
    if (target.name.empty()) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "Target name is empty");
    }

    // Pass the target name to the container engine to kill it.
    // IContainerEngine::killContainer takes a std::string_view
    try {
        engine.killContainer(target.name);
    } catch (const std::exception& e) {
        // Wrap any underlying exception in a system_error as mandated by the interface.
        // If the engine itself doesn't throw system_error, we adapt it here.
        throw std::system_error(std::make_error_code(std::errc::operation_canceled), e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
