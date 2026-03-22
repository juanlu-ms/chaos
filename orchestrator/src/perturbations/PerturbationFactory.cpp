#include "perturbations/PerturbationFactory.hpp"

#include <stdexcept>
#include <utility>

#include "perturbations/CpuCapPerturbation.hpp"
#include "perturbations/KillPerturbation.hpp"
#include "perturbations/MemoryCapPerturbation.hpp"
#include "perturbations/NetworkDelayPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

std::unique_ptr<IPerturbation> PerturbationFactory::create(std::shared_ptr<containers::IContainerEngine> engine,
                                                           const manifests::Target& target,
                                                           const manifests::Perturbation& spec) {
    if (spec.type == "kill") {
        return std::make_unique<KillPerturbation>(std::move(engine), target.id);
    } else if (spec.type == "memory_cap") {
        return std::make_unique<MemoryCapPerturbation>(std::move(engine), target.id, spec);
    } else if (spec.type == "cpu_cap") {
        return std::make_unique<CpuCapPerturbation>(std::move(engine), target.id, spec);
    } else if (spec.type == "network_delay") {
        return std::make_unique<NetworkDelayPerturbation>(std::move(engine), target.id, spec);
    }

    // Fallback for unimplemented types
    throw std::invalid_argument("Unsupported perturbation type or type not yet implemented: " + spec.type);
}

}  // namespace chaos::orchestrator::perturbations
