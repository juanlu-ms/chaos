/// @file GarbagePacketPerturbation.cpp
/// @brief Implements garbage packet injection via tc netem in the container's network namespace.

#include "perturbations/GarbagePacketPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "PerturbationUtils.hpp"

namespace chaos::orchestrator::perturbations {

GarbagePacketPerturbation::GarbagePacketPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                                     std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

/**
 * @brief Applies tc netem rules inside the container's network namespace.
 *
 * Builds a composite netem qdisc rule including any combination of:
 * - corrupt: random bit corruption ("corrupt_pct", e.g. "5%")
 * - loss: packet loss ("loss_pct", e.g. "10%")
 * - duplicate: packet duplication ("duplicate_pct", e.g. "3%")
 *
 * Interface defaults to "eth0" unless "iface" parameter is specified.
 *
 * @throws std::invalid_argument If target ID is empty or no parameters provided.
 * @throws std::system_error On tc command failure.
 */
void GarbagePacketPerturbation::apply() {
    if (hasBeenApplied_) {
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

    const std::string opts = netemOpts.str();
    if (opts.empty()) {
        throw std::invalid_argument(
            "GarbagePacketPerturbation requires at least one parameter: corrupt_pct, loss_pct, or duplicate_pct");
    }

    const std::string iface = [&] {
        auto ifaceIt = params_.find("iface");
        return (ifaceIt != params_.end()) ? ifaceIt->second : std::string("eth0");
    }();

    try {
        const std::string pid = internal::fetchContainerPid(target_id_);
        const std::string nsenter = "nsenter -t " + pid + " -n -- ";

        internal::runCommand(fmt::format("{}tc qdisc add dev {} root netem{}", nsenter, iface, opts),
                             "Failed to apply tc netem rule");

        hasBeenApplied_ = true;
        SPDLOG_INFO("Garbage Packet Perturbation applied on target {} iface={}: {}", target_id_, iface, opts);

    } catch (const std::system_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), e.what());
    }
}

/**
 * @brief Reverts tc netem rules inside the container's network namespace.
 *
 * Deletes the root qdisc from the configured interface (eth0 by default),
 * which restores normal packet flow.
 *
 * @throws std::system_error On tc command failure.
 */
void GarbagePacketPerturbation::revert() {
    if (!hasBeenApplied_) {
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
        const std::string pid = internal::fetchContainerPid(target_id_);
        const std::string nsenter = "nsenter -t " + pid + " -n -- ";

        internal::runCommand(nsenter + "tc qdisc del dev " + iface + " root netem", "Failed to revert tc netem rule");

        hasBeenApplied_ = false;
        SPDLOG_INFO("Garbage Packet Perturbation reverted on target {}", target_id_);

    } catch (const std::system_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
