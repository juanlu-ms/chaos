#include "perturbations/PerturbationFactory.hpp"

#include <stdexcept>

#include "perturbations/KillPerturbation.hpp"
#include "perturbations/MemoryCapPerturbation.hpp"
#include "perturbations/CpuCapPerturbation.hpp"
#include "perturbations/NetworkCapPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

std::unique_ptr<IPerturbation> PerturbationFactory::create(const manifests::Perturbation& spec) {
    if (spec.type == "kill") {
        return std::make_unique<KillPerturbation>();
    } else if (spec.type == "memory_cap") {
        return std::make_unique<MemoryCapPerturbation>(spec.parameters);
    } else if (spec.type == "cpu_cap") {
        return std::make_unique<CpuCapPerturbation>(spec.parameters);
    } else if (spec.type == "network_cap") {
        return std::make_unique<NetworkCapPerturbation>(spec.parameters);
    }
    
    // Fallback for unimplemented types
    throw std::invalid_argument("Unsupported perturbation type or type not yet implemented: " + spec.type);
}

}  // namespace chaos::orchestrator::perturbations
