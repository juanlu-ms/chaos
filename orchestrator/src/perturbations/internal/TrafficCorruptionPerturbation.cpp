#include "TrafficCorruptionPerturbation.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::perturbations {

namespace {
constexpr auto kDefaultIface = "eth0";
}

TrafficCorruptionPerturbation::TrafficCorruptionPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                                             std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

void TrafficCorruptionPerturbation::apply() {
    if (bool expected = false; !hasBeenApplied_.compare_exchange_strong(expected, true)) {
        SPDLOG_WARN("Traffic Corruption already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    std::string opts;
    if (auto iter = params_.find("corrupt_pct"); iter != params_.end()) {
        opts += fmt::format(" corrupt {}", iter->second);
    }
    if (auto iter = params_.find("loss_pct"); iter != params_.end()) {
        opts += fmt::format(" loss {}", iter->second);
    }
    if (auto iter = params_.find("duplicate_pct"); iter != params_.end()) {
        opts += fmt::format(" duplicate {}", iter->second);
    }
    if (opts.empty()) {
        opts = " corrupt 100";
    }

    const std::string iface = [&] {
        auto ifaceIt = params_.find("iface");
        return (ifaceIt != params_.end()) ? ifaceIt->second : std::string(kDefaultIface);
    }();

    try {
        {
            const auto execOut =
                engine_->execInNetNs(target_id_, fmt::format("tc qdisc replace dev {} root netem{}", iface, opts));
            if (!execOut.empty()) {
                SPDLOG_DEBUG("tc output: {}", execOut);
            }
        }
        SPDLOG_INFO("Traffic Corruption applied on target {} iface={}: {}", target_id_, iface, opts);
    } catch (const containers::ContainerEngineError& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to apply tc netem rule inside container: ") + e.what());
    }
}

void TrafficCorruptionPerturbation::revert() {
    if (bool expected = true; !hasBeenApplied_.compare_exchange_strong(expected, false)) {
        SPDLOG_WARN("Traffic Corruption was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        hasBeenApplied_ = true;
        throw std::invalid_argument("Target ID is empty");
    }

    const std::string iface = [&] {
        auto ifaceIt = params_.find("iface");
        return (ifaceIt != params_.end()) ? ifaceIt->second : std::string(kDefaultIface);
    }();

    try {
        {
            const auto execOut = engine_->execInNetNs(target_id_, "tc qdisc del dev " + iface + " root netem");
            if (!execOut.empty()) {
                SPDLOG_DEBUG("tc output: {}", execOut);
            }
        }
        SPDLOG_INFO("Traffic Corruption reverted on target {}", target_id_);
    } catch (const containers::ContainerEngineError& e) {
        hasBeenApplied_ = true;
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                std::string("Failed to revert tc netem rule inside container: ") + e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
