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

NetworkMetrics parseProcNetDev(int pid, uint64_t& prev_rx, uint64_t& prev_tx,
                               std::chrono::steady_clock::time_point& prev_time, bool& prev_valid) {
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
    if (prev_valid) {
        auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - prev_time).count();
        if (dt > 0.0) {
            metrics.rx_bps = static_cast<double>(rx - prev_rx) / dt;
            metrics.tx_bps = static_cast<double>(tx - prev_tx) / dt;
        }
    }
    prev_rx = rx;
    prev_tx = tx;
    prev_time = now;
    prev_valid = true;

    return metrics;
}

std::optional<double> executePing(std::string_view target_ip) {
    std::string cmd = fmt::format("LC_ALL=C ping -c 1 -W 2 {} 2>&1", target_ip);

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

ObservationLoop::ObservationLoop(std::shared_ptr<containers::IContainerEngine> engine, std::string container_id,
                                 SharedState& state, IRunObserver& observer, Config config)
    : engine_(std::move(engine)),
      container_id_(std::move(container_id)),
      state_(state),
      observer_(observer),
      config_(std::move(config)) {}

ObservationLoop::~ObservationLoop() { internalStopSource_.request_stop(); }

void ObservationLoop::start(std::stop_token external_stop) {
    cgroup_ = std::make_unique<containers::CgroupMetricsGatherer>();
    if (!containers::CgroupMetricsGatherer::resolveCgroupPath(container_id_).has_value()) {
        cgroup_.reset();
    } else {
        SPDLOG_DEBUG("ObservationLoop: using cgroup v2 for {}", container_id_);
    }

    observability::ObservabilityEngine init_obs(engine_);
    containerIp_ = init_obs.getContainerIp(container_id_);

    try {
        container_pid_ = engine_->getContainerPid(container_id_);
    } catch (const containers::ContainerEngineError& e) {
        SPDLOG_DEBUG("ObservationLoop: could not get container PID: {}", e.what());
    }

    auto initial = init_obs.observe(container_id_);
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
    auto last_continuous_check = std::chrono::steady_clock::now();

    uint64_t prev_net_rx{0};
    uint64_t prev_net_tx{0};
    auto prev_net_time = std::chrono::steady_clock::now();
    bool prev_net_valid{false};

    while (!internal_stop.stop_requested() && !external_stop.stop_requested()) {
        auto tick = std::chrono::steady_clock::now();

        try {
            core::TargetState state;
            state.container_id = container_id_;
            if (containerIp_) {
                state.container_ip = *containerIp_;
            }

            state.status = obs.getStatus(container_id_);

            if (state.status == containers::ContainerStatus::Running) {
                if (cgroup_) {
                    auto cpu = cgroup_->getCpuUsagePercent(container_id_);
                    auto mem = cgroup_->getMemoryUsageMb(container_id_);
                    if (cpu.has_value()) {
                        state.cpu_usage_percent = cpu;
                    }
                    if (mem.has_value()) {
                        state.memory_usage_mb = mem;
                    }

                    if (container_pid_ > 0) {
                        bool had_previous = prev_net_valid;
                        auto net = detail::parseProcNetDev(container_pid_, prev_net_rx, prev_net_tx, prev_net_time,
                                                           prev_net_valid);
                        if (had_previous) {
                            state.network_rx_bps = net.rx_bps;
                            state.network_tx_bps = net.tx_bps;
                        }
                    }
                } else {
                    try {
                        auto stats = obs.getStats(container_id_);
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

            if (auto time_since_last_check = tick - last_continuous_check;
                time_since_last_check >= config_.continuousValidationInterval &&
                !config_.continuous_expectations.empty()) {
                last_continuous_check = tick;

                std::vector<manifests::Expectation> continuous;
                std::ranges::copy_if(config_.continuous_expectations, std::back_inserter(continuous),
                                     [](const auto& e) { return e.continuous; });

                if (!continuous.empty()) {
                    auto results = validation::validate(state_.latestState(), continuous);
                    for (const auto& r : results) {
                        if (!r.passed) {
                            state_.addContinuousFailure(r.expectation_type);
                            SPDLOG_WARN("Continuous expectation '{}' failed", r.expectation_type);
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
            auto raw_logs = obs.getLogs(container_id_);
            std::vector<std::string> log_lines;
            observability::parseLogLines(raw_logs, log_lines);
            state_.updateLogs(log_lines);
            observer_.onLogsUpdate(log_lines);
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
