#include "GarbagePacketPerturbation.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::perturbations {

GarbagePacketPerturbation::GarbagePacketPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                                     std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

void GarbagePacketPerturbation::apply() {
    if (bool expected = false; !hasBeenApplied_.compare_exchange_strong(expected, true)) {
        SPDLOG_WARN("Garbage Packet Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    // Build netem options string
    std::ostringstream netemOpts;

    auto it = params_.find("corrupt_pct");
    if (it != params_.end()) {
        netemOpts << " corrupt " << it->second;
    }

    it = params_.find("loss_pct");
    if (it != params_.end()) {
        netemOpts << " loss " << it->second;
    }

    it = params_.find("duplicate_pct");
    if (it != params_.end()) {
        netemOpts << " duplicate " << it->second;
    }

    std::string opts = netemOpts.str();
    if (opts.empty()) {
        opts = " corrupt 100";
    }

    const std::string iface = [&] {
        auto ifaceIt = params_.find("iface");
        return (ifaceIt != params_.end()) ? ifaceIt->second : std::string("eth0");
    }();

    try {
        (void)engine_->execInNetNs(target_id_, fmt::format("tc qdisc add dev {} root netem{}", iface, opts));
        SPDLOG_INFO("Garbage Packet Perturbation applied on target {} iface={}: {}", target_id_, iface, opts);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply tc netem rule inside container: ") + e.what());
    }
}

void GarbagePacketPerturbation::revert() {
    if (bool expected = true; !hasBeenApplied_.compare_exchange_strong(expected, false)) {
        SPDLOG_WARN("Garbage Packet Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    const std::string iface = [&] {
        auto ifaceIt = params_.find("iface");
        return (ifaceIt != params_.end()) ? ifaceIt->second : std::string("eth0");
    }();

    try {
        (void)engine_->execInNetNs(target_id_, "tc qdisc del dev " + iface + " root netem");
        SPDLOG_INFO("Garbage Packet Perturbation reverted on target {}", target_id_);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert tc netem rule inside container: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
