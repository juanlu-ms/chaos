#include "perturbations/PerturbationFactory.hpp"

#include <stdexcept>
#include <utility>

#include "internal/CpuCapPerturbation.hpp"
#include "internal/KillPerturbation.hpp"
#include "internal/MemoryCapPerturbation.hpp"
#include "internal/NetworkCutoffPerturbation.hpp"
#include "internal/NetworkDelayPerturbation.hpp"
#include "internal/PacketFloodPerturbation.hpp"
#include "internal/TrafficCorruptionPerturbation.hpp"

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
    } else if (spec.type == "network_cutoff") {
        return std::make_unique<NetworkCutoffPerturbation>(std::move(engine), target.id, spec);
    } else if (spec.type == "traffic_corruption") {
        return std::make_unique<TrafficCorruptionPerturbation>(std::move(engine), target.id, spec);
    } else if (spec.type == "packet_flood") {
        return std::make_unique<PacketFloodPerturbation>(std::move(engine), target.id, spec);
    }

    throw std::invalid_argument("Unsupported perturbation type or type not yet implemented: " + spec.type);
}

}  // namespace chaos::orchestrator::perturbations
