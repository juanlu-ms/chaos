#include "history/RunRecorder.hpp"

#include <chrono>
#include <format>
#include <string>
#include <vector>

namespace chaos::orchestrator::history {

using namespace std::chrono;

RunRecorder::RunRecorder(manifests::ChaosManifest manifest)
    : manifest_(std::move(manifest)), start_steady_(steady_clock::now()), start_system_(system_clock::now()) {
    auto ms = duration_cast<milliseconds>(start_system_.time_since_epoch()).count();
    summary_.id = std::format("run-{}", ms);
    summary_.started_at_unix = ms;
    for (const auto& p : manifest_.perturbations) {
        summary_.perturbation_types.push_back(p.type);
    }
}

void RunRecorder::onStateUpdate(const core::TargetState& state) {
    std::lock_guard lock(mutex_);
    double t = duration<double>(steady_clock::now() - start_steady_).count();
    RunSample sample;
    sample.t = t;
    sample.cpu_usage_percent = state.cpu_usage_percent;
    sample.memory_usage_mb = state.memory_usage_mb;
    sample.network_rx_bps = state.network_rx_bps;
    sample.network_tx_bps = state.network_tx_bps;
    sample.network_latency_ms = last_latency_;
    sample.phase = phase_;
    samples_.push_back(std::move(sample));
}

void RunRecorder::onNetworkLatencyUpdate(std::optional<double> latency) {
    std::lock_guard lock(mutex_);
    last_latency_ = latency;
}

void RunRecorder::onPhaseChange(std::string_view phase) {
    std::lock_guard lock(mutex_);
    double elapsed = duration<double>(steady_clock::now() - start_steady_).count();
    if (phase_ == "normal" && phase == "chaos") {
        summary_.normal_end_t = elapsed;
    } else if (phase_ == "chaos" && phase == "recovery") {
        summary_.chaos_end_t = elapsed;
    }
    phase_ = phase;
}

void RunRecorder::onLogsUpdate(const std::vector<std::string>& logs) {
    std::lock_guard lock(mutex_);
    logs_ = logs;
}

RunRecord RunRecorder::finalize(const core::RunResult& run_result, std::string status, std::string error) {
    std::lock_guard lock(mutex_);
    summary_.ended_at_unix = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    summary_.run_result = run_result;
    summary_.status = std::move(status);
    summary_.error = std::move(error);
    return RunRecord{.summary = std::move(summary_),
                     .manifest = std::move(manifest_),
                     .samples = std::move(samples_),
                     .logs = std::move(logs_)};
}

}  // namespace chaos::orchestrator::history
