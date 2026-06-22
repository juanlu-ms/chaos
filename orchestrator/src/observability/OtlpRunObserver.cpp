#include "observability/OtlpRunObserver.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

#include "core/RunResult.hpp"
#include "core/TargetState.hpp"
#include "validation/ValidationResult.hpp"

namespace chaos::orchestrator::observability {

using json = nlohmann::json;

OtlpRunObserver::OtlpRunObserver(OtlpExporter exporter) : exporter_(std::move(exporter)) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count();
    run_id_ = "run-" + std::to_string(ms);
    last_state_export_ = std::chrono::steady_clock::time_point{};
}

void OtlpRunObserver::onPhaseChange(std::string_view phase) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        phase_ = phase;
        json body = {{"phase", std::string(phase)}};
        auto payload = OtlpExporter::buildLogPayload(run_id_, "phase_change", body.dump(),
                                                     {{"service.name", "chaos-orchestrator"}});
        if (!exporter_.exportLogs(payload)) {
            SPDLOG_WARN("OTLP export failed for phase_change");
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("OtlpRunObserver::onPhaseChange: {}", e.what());
    } catch (...) {
        SPDLOG_ERROR("OtlpRunObserver::onPhaseChange: unknown error");
    }
}

void OtlpRunObserver::onStateUpdate(const core::TargetState& state) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        if (now - last_state_export_ < state_throttle_) {
            return;
        }
        last_state_export_ = now;

        json body;
        body["cpu_percent"] = state.cpu_usage_percent.has_value() ? json(*state.cpu_usage_percent) : json();
        body["memory_mb"] = state.memory_usage_mb.has_value() ? json(*state.memory_usage_mb) : json();
        body["net_rx_bps"] = state.network_rx_bps.has_value() ? json(*state.network_rx_bps) : json();
        body["net_tx_bps"] = state.network_tx_bps.has_value() ? json(*state.network_tx_bps) : json();
        body["phase"] = phase_;
        body["container_id"] = state.container_id;

        auto payload = OtlpExporter::buildLogPayload(run_id_, "state_sample", body.dump(),
                                                     {{"service.name", "chaos-orchestrator"}});
        if (!exporter_.exportLogs(payload)) {
            SPDLOG_WARN("OTLP export failed for state_sample");
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("OtlpRunObserver::onStateUpdate: {}", e.what());
    } catch (...) {
        SPDLOG_ERROR("OtlpRunObserver::onStateUpdate: unknown error");
    }
}

bool OtlpRunObserver::finalize(const core::RunResult& result) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        int failureCount = 0;
        for (const auto& r : result.results) {
            if (!r.passed) {
                ++failureCount;
            }
        }

        std::string severity = result.passed ? "run_complete" : "run_failed";

        json body;
        body["passed"] = result.passed;
        body["failures"] = failureCount;
        body["duration_s"] = result.duration_s;
        body["manifest"] = result.manifest_name;
        body["target_id"] = result.target_id;

        auto payload =
            OtlpExporter::buildLogPayload(run_id_, severity, body.dump(), {{"service.name", "chaos-orchestrator"}});
        if (!exporter_.exportLogs(payload)) {
            SPDLOG_WARN("OTLP export failed for finalize");
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("OtlpRunObserver::finalize: {}", e.what());
        return false;
    } catch (...) {
        SPDLOG_ERROR("OtlpRunObserver::finalize: unknown error");
        return false;
    }
}

}  // namespace chaos::orchestrator::observability
