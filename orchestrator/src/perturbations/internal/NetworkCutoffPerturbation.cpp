/// @file NetworkCutoffPerturbation.cpp
/// @brief Implements network cutoff via iptables rules in the container's network namespace.

#include "perturbations/NetworkCutoffPerturbation.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace chaos::orchestrator::perturbations {

namespace {

/**
 * @brief Fetches the host-level PID of a container using 'docker inspect'.
 * @param containerId Docker container ID or name.
 * @return PID as a string.
 * @throws std::runtime_error On failure.
 */
std::string fetchContainerPid(const std::string& containerId) {
    std::string cmd = "docker inspect --format '{{.State.Pid}}' " + containerId;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to run docker inspect to fetch PID");
    }

    char buffer[64];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    while (!result.empty() && (result.back() == '\n' || result.back() == ' ')) {
        result.pop_back();
    }

    if (result.empty() || result == "0") {
        throw std::runtime_error("Failed to resolve container PID for: " + containerId);
    }

    return result;
}

/**
 * @brief Runs a shell command and throws on failure.
 * @param cmd Shell command to execute.
 * @param errorMsg Error message to embed in the exception on failure.
 * @throws std::system_error If the command exits with non-zero.
 */
void runCommand(const std::string& cmd, const std::string& errorMsg) {
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), errorMsg);
    }
}

}  // namespace

NetworkCutoffPerturbation::NetworkCutoffPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                                     std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)), params_(spec.parameters) {}

/**
 * @brief Applies iptables DROP rules inside the container's network namespace.
 *
 * Strategy: Use nsenter to inject iptables rules into the container's netns.
 * - If "dst_ip" param: block `OUTPUT -d <ip>`
 * - If "dst_port" param: block `OUTPUT -p tcp --dport <port>`
 * - If "src_port" param: block `INPUT -p tcp --dport <port>`
 * - If no filter params: block all traffic (OUTPUT + INPUT chains to DROP)
 *
 * All applied rules are tracked so they can be removed precisely on revert.
 *
 * @throws std::invalid_argument If target ID is empty.
 * @throws std::system_error On command failure.
 */
void NetworkCutoffPerturbation::apply() {
    if (hasBeenApplied_) {
        SPDLOG_WARN("Network Cutoff Perturbation already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        const std::string pid = fetchContainerPid(target_id_);
        const std::string nsenter = "nsenter -t " + pid + " -n -- ";

        bool hasFilter = false;

        auto addRule = [&](const std::string& applyArgs, const std::string& revertArgs) {
            runCommand(nsenter + "iptables " + applyArgs,
                       "Failed to apply iptables rule: " + applyArgs);
            revertCommands_.push_back(nsenter + "iptables " + revertArgs);
            hasFilter = true;
        };

        auto it = params_.find("dst_ip");
        if (it != params_.end()) {
            addRule("-A OUTPUT -d " + it->second + " -j DROP",
                    "-D OUTPUT -d " + it->second + " -j DROP");
        }

        it = params_.find("dst_port");
        if (it != params_.end()) {
            addRule("-A OUTPUT -p tcp --dport " + it->second + " -j DROP",
                    "-D OUTPUT -p tcp --dport " + it->second + " -j DROP");
        }

        it = params_.find("src_port");
        if (it != params_.end()) {
            addRule("-A INPUT -p tcp --dport " + it->second + " -j DROP",
                    "-D INPUT -p tcp --dport " + it->second + " -j DROP");
        }

        if (!hasFilter) {
            // No filters: block all egress and ingress
            addRule("-A OUTPUT -j DROP", "-D OUTPUT -j DROP");
            addRule("-A INPUT -j DROP", "-D INPUT -j DROP");
        }

        hasBeenApplied_ = true;
        SPDLOG_INFO("Network Cutoff Perturbation applied on target {}", target_id_);

    } catch (const std::system_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), e.what());
    }
}

/**
 * @brief Reverts applied iptables rules inside the container's network namespace.
 *
 * Removes exactly the rules that were created during apply(), preserving any
 * other iptables rules that may have existed before, allowing the container
 * to reconnect normally.
 *
 * @throws std::system_error On command failure.
 */
void NetworkCutoffPerturbation::revert() {
    if (!hasBeenApplied_) {
        SPDLOG_WARN("Network Cutoff Perturbation was not applied, skipping revert");
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    try {
        for (const auto& cmd : revertCommands_) {
            runCommand(cmd, "Failed to revert iptables rule: " + cmd);
        }

        revertCommands_.clear();
        hasBeenApplied_ = false;
        SPDLOG_INFO("Network Cutoff Perturbation reverted on target {}", target_id_);

    } catch (const std::system_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported), e.what());
    }
}

}  // namespace chaos::orchestrator::perturbations
