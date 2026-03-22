#include "perturbations/PerturbationFactory.hpp"

#include <stdexcept>

#include "perturbations/CpuCapPerturbation.hpp"
#include "perturbations/KillPerturbation.hpp"
#include "perturbations/MemoryCapPerturbation.hpp"
#include "perturbations/NetworkCapPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

std::unique_ptr<IPerturbation> PerturbationFactory::create(std::shared_ptr<containers::IContainerEngine> engine,
                                                           const manifests::Perturbation& spec) {
    if (spec.type == "kill") {
        return std::make_unique<KillPerturbation>(engine);
    } else if (spec.type == "memory_cap") {
        return std::make_unique<MemoryCapPerturbation>(engine, spec.parameters);
    } else if (spec.type == "cpu_cap") {
        return std::make_unique<CpuCapPerturbation>(engine, spec.parameters);
    } else if (spec.type == "network_cap") {
        return std::make_unique<NetworkCapPerturbation>(engine, spec.parameters);
    }

    // Fallback for unimplemented types
    throw std::invalid_argument("Unsupported perturbation type or type not yet implemented: " + spec.type);
}

}  // namespace chaos::orchestrator::perturbations
