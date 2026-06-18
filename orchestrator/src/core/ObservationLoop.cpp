#include "core/ObservationLoop.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

#include "containers/internal/CgroupMetricsGatherer.hpp"
#include "core/internal/ObservationLoopDetail.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::core::detail {

NetworkMetrics parseProcNetDev(int pid, uint64_t& prevRx, uint64_t& prevTx,
                               std::chrono::steady_clock::time_point& prevTime, bool& prevValid) {
    NetworkMetrics metrics;

    std::ifstream file(fmt::format("/proc/{}/net/dev", pid));
    if (!file.is_open()) {
        return metrics;
    }

    uint64_t rx{0};
    uint64_t tx{0};
    std::string line;
    while (std::getline(file, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string iface = line.substr(0, colon);
        if (auto start = iface.find_first_not_of(" \t"); start != std::string::npos) {
            iface = iface.substr(start);
        }
        if (iface != "eth0") {
            continue;
        }

        std::istringstream iss(line.substr(colon + 1));
        uint64_t rbytes{0};
        uint64_t rpackets{0};
        uint64_t rerrs{0};
        uint64_t rdrop{0};
        uint64_t rfifo{0};
        uint64_t rframe{0};
        uint64_t rcompressed{0};
        uint64_t rmulticast{0};
        uint64_t tbytes{0};
        iss >> rbytes >> rpackets >> rerrs >> rdrop >> rfifo >> rframe >> rcompressed >> rmulticast >> tbytes;
        rx = rbytes;
        tx = tbytes;
        break;
    }

    const auto now = std::chrono::steady_clock::now();
    if (prevValid) {
        auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - prevTime).count();
        if (dt > 0.0) {
            metrics.rxBps = static_cast<double>(rx - prevRx) / dt;
            metrics.txBps = static_cast<double>(tx - prevTx) / dt;
        }
    }
    prevRx = rx;
    prevTx = tx;
    prevTime = now;
    prevValid = true;

    return metrics;
}

std::optional<double> executePing(std::string_view targetIp) {
    std::string cmd = fmt::format("LC_ALL=C ping -c 1 -W 2 {} 2>&1", targetIp);

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }

    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    int rc = pclose(pipe);

    if (rc != 0) {
        return std::nullopt;
    }

    std::regex re(R"(time=([0-9.]+)\s*ms)");
    std::smatch match;
    if (!std::regex_search(output, match, re)) {
        return std::nullopt;
    }

    try {
        return std::stod(match[1].str());
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void interruptibleSleep(std::chrono::seconds duration, const std::stop_token& stop) {
    for (int i = 0; i < static_cast<int>(duration.count() * 10) && !stop.stop_requested(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

}  // namespace chaos::orchestrator::core::detail

namespace chaos::orchestrator::core {

using namespace std::chrono_literals;

ObservationLoop::ObservationLoop(std::shared_ptr<containers::IContainerEngine> engine, std::string containerId,
                                 SharedState& state, IRunObserver& observer, Config config)
    : engine_(std::move(engine)),
      containerId_(std::move(containerId)),
      state_(state),
      observer_(observer),
      config_(std::move(config)) {}

ObservationLoop::~ObservationLoop() { internalStopSource_.request_stop(); }

void ObservationLoop::start(std::stop_token external_stop) {
    cgroup_ = std::make_unique<containers::CgroupMetricsGatherer>();
    if (!containers::CgroupMetricsGatherer::resolveCgroupPath(containerId_).has_value()) {
        cgroup_.reset();
    } else {
        SPDLOG_DEBUG("ObservationLoop: using cgroup v2 for {}", containerId_);
    }

    observability::ObservabilityEngine initObs(engine_);
    containerIp_ = initObs.getContainerIp(containerId_);

    try {
        containerPid_ = engine_->getContainerPid(containerId_);
    } catch (const containers::ContainerEngineError& e) {
        SPDLOG_DEBUG("ObservationLoop: could not get container PID: {}", e.what());
    }

    auto initial = initObs.observe(containerId_);
    if (containerIp_) {
        initial.container_ip = *containerIp_;
    }
    state_.updateState(initial);
    observer_.onStateUpdate(initial);

    auto internal_stop = internalStopSource_.get_token();

    metricsThread_ =
        std::jthread([this, internal_stop, external_stop] { metricsThreadFn(internal_stop, external_stop); });

    logsThread_ = std::jthread([this, internal_stop, external_stop] { logsThreadFn(internal_stop, external_stop); });

    latencyThread_ =
        std::jthread([this, internal_stop, external_stop] { latencyThreadFn(internal_stop, external_stop); });
}

void ObservationLoop::metricsThreadFn(std::stop_token internal_stop, std::stop_token external_stop) {
    observability::ObservabilityEngine obs(engine_);
    auto lastContinuousCheck = std::chrono::steady_clock::now();

    uint64_t prevNetRx{0};
    uint64_t prevNetTx{0};
    auto prevNetTime = std::chrono::steady_clock::now();
    bool prevNetValid{false};

    while (!internal_stop.stop_requested() && !external_stop.stop_requested()) {
        auto tick = std::chrono::steady_clock::now();

        try {
            core::TargetState state;
            state.container_id = containerId_;
            if (containerIp_) {
                state.container_ip = *containerIp_;
            }

            state.status = obs.getStatus(containerId_);

            if (state.status == containers::ContainerStatus::Running) {
                if (cgroup_) {
                    auto cpu = cgroup_->getCpuUsagePercent(containerId_);
                    auto mem = cgroup_->getMemoryUsageMb(containerId_);
                    if (cpu.has_value()) {
                        state.cpu_usage_percent = cpu;
                    }
                    if (mem.has_value()) {
                        state.memory_usage_mb = mem;
                    }

                    if (containerPid_ > 0) {
                        bool hadPrevious = prevNetValid;
                        auto net =
                            detail::parseProcNetDev(containerPid_, prevNetRx, prevNetTx, prevNetTime, prevNetValid);
                        if (hadPrevious) {
                            state.network_rx_bps = net.rxBps;
                            state.network_tx_bps = net.txBps;
                        }
                    }
                } else {
                    try {
                        auto stats = obs.getStats(containerId_);
                        state.cpu_usage_percent = stats.cpu_percent;
                        state.memory_usage_mb = stats.memory_mb;
                        state.network_rx_bps = stats.network_rx_bps;
                        state.network_tx_bps = stats.network_tx_bps;
                    } catch (const containers::ContainerEngineError& e) {
                        SPDLOG_ERROR("Metrics thread: failed to fetch stats: {}", e.what());
                    }
                }
            }

            state_.updateMetrics(state);
            observer_.onStateUpdate(state);

            if (auto timeSinceLastCheck = tick - lastContinuousCheck;
                timeSinceLastCheck >= config_.continuousValidationInterval && !config_.continuousExpectations.empty()) {
                lastContinuousCheck = tick;

                std::vector<manifests::Expectation> continuous;
                std::ranges::copy_if(config_.continuousExpectations, std::back_inserter(continuous),
                                     [](const auto& e) { return e.continuous; });

                if (!continuous.empty()) {
                    auto results = validation::validate(state_.latestState(), continuous);
                    for (const auto& r : results) {
                        if (!r.passed) {
                            state_.addContinuousFailure(r.expectationType);
                            SPDLOG_WARN("Continuous expectation '{}' failed", r.expectationType);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Metrics thread: unhandled exception — {}", e.what());
        }

        auto elapsed = std::chrono::steady_clock::now() - tick;
        auto remaining = config_.metricsInterval - elapsed;
        if (remaining > 0ms) {
            std::this_thread::sleep_for(remaining);
        }
    }
}

void ObservationLoop::logsThreadFn(std::stop_token internal_stop, std::stop_token external_stop) {
    observability::ObservabilityEngine obs(engine_);

    while (!internal_stop.stop_requested() && !external_stop.stop_requested()) {
        auto tick = std::chrono::steady_clock::now();

        try {
            auto rawLogs = obs.getLogs(containerId_);
            std::vector<std::string> logLines;
            observability::parseLogLines(rawLogs, logLines);
            state_.updateLogs(logLines);
            observer_.onLogsUpdate(logLines);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Logs thread: unhandled exception — {}", e.what());
        }

        auto elapsed = std::chrono::steady_clock::now() - tick;
        auto remaining = config_.logsInterval - elapsed;
        if (remaining > 0ms) {
            std::this_thread::sleep_for(remaining);
        }
    }
}

void ObservationLoop::latencyThreadFn(std::stop_token internal_stop, std::stop_token external_stop) {
    if (containerIp_.has_value() && !containerIp_->empty()) {
        SPDLOG_DEBUG("Latency monitoring started for {}", *containerIp_);
    }

    while (!internal_stop.stop_requested() && !external_stop.stop_requested()) {
        std::optional<double> latency;

        if (containerIp_.has_value() && !containerIp_->empty()) {
            latency = detail::executePing(*containerIp_);
        }

        state_.updateNetworkLatency(latency);
        observer_.onNetworkLatencyUpdate(latency);

        detail::interruptibleSleep(1s, internal_stop);
    }
}

}  // namespace chaos::orchestrator::core
