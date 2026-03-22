#include "perturbations/PerturbationFactory.hpp"

#include <stdexcept>

#include "perturbations/CpuCapPerturbation.hpp"
#include "perturbations/KillPerturbation.hpp"
#include "perturbations/MemoryCapPerturbation.hpp"
#include "perturbations/NetworkDelayPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

std::unique_ptr<IPerturbation> PerturbationFactory::create(std::shared_ptr<containers::IContainerEngine> engine,
                                                           const manifests::Perturbation& spec) {
    if (spec.type == "kill") {
        return std::make_unique<KillPerturbation>(engine);
    } else if (spec.type == "memory_cap") {
        return std::make_unique<MemoryCapPerturbation>(engine, spec);
    } else if (spec.type == "cpu_cap") {
        return std::make_unique<CpuCapPerturbation>(engine, spec);
    } else if (spec.type == "network_delay") {
        return std::make_unique<NetworkDelayPerturbation>(engine, spec);
    }

    // Fallback for unimplemented types
    throw std::invalid_argument("Unsupported perturbation type or type not yet implemented: " + spec.type);
}

}  // namespace chaos::orchestrator::perturbations
