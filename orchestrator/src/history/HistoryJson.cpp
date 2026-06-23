#include "history/HistoryJson.hpp"

#include <spdlog/spdlog.h>

#include <chrono>

#include "core/ResultSerializer.hpp"
#include "history/RunRecord.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::history {

nlohmann::json runSummaryToJson(const RunSummary& summary) {
    auto j = core::runResultToJson(summary.run_result);
    j["id"] = summary.id;
    j["started_at_unix"] = summary.started_at_unix;
    j["ended_at_unix"] = summary.ended_at_unix;
    j["status"] = summary.status;
    j["error"] = summary.error;
    j["perturbation_types"] = nlohmann::json(summary.perturbation_types);
    j["normal_end_t"] = summary.normal_end_t;
    j["chaos_end_t"] = summary.chaos_end_t;
    return j;
}

namespace {
void putIfPresent(nlohmann::json& j, const char* key, const std::optional<double>& val) {
    if (val) j[key] = *val;
}
}  // namespace

nlohmann::json runRecordToJson(const RunRecord& record) {
    nlohmann::json j;
    j["summary"] = runSummaryToJson(record.summary);
    j["manifest"]["test_name"] = record.manifest.test_name;
    j["manifest"]["target_id"] = record.manifest.target.id;
    j["manifest"]["duration_s"] =
        record.manifest.duration_s.has_value() ? nlohmann::json(*record.manifest.duration_s) : nlohmann::json(nullptr);
    j["manifest"]["perturbations"] = nlohmann::json::array();
    for (const auto& pert : record.manifest.perturbations) {
        nlohmann::json pert_json;
        pert_json["type"] = pert.type;
        pert_json["parameters"] = nlohmann::json(pert.parameters);
        j["manifest"]["perturbations"].push_back(std::move(pert_json));
    }
    j["manifest"]["expectations"] = nlohmann::json::array();
    for (const auto& exp : record.manifest.expectations) {
        nlohmann::json exp_json;
        exp_json["type"] = exp.type;
        exp_json["parameters"] = nlohmann::json(exp.parameters);
        exp_json["continuous"] = exp.continuous;
        j["manifest"]["expectations"].push_back(std::move(exp_json));
    }
    j["samples"] = nlohmann::json::array();
    for (const auto& sample : record.samples) {
        nlohmann::json s;
        s["t"] = sample.t;
        s["phase"] = sample.phase;
        putIfPresent(s, "cpu_usage_percent", sample.cpu_usage_percent);
        putIfPresent(s, "memory_usage_mb", sample.memory_usage_mb);
        putIfPresent(s, "network_rx_bps", sample.network_rx_bps);
        putIfPresent(s, "network_tx_bps", sample.network_tx_bps);
        putIfPresent(s, "network_latency_ms", sample.network_latency_ms);
        j["samples"].push_back(std::move(s));
    }
    j["logs"] = nlohmann::json(record.logs);
    return j;
}

std::optional<RunSummary> runSummaryFromJson(const nlohmann::json& j) {
    try {
        RunSummary summary;
        summary.id = j.at("id").get<std::string>();
        summary.started_at_unix = j.at("started_at_unix").get<int64_t>();
        summary.ended_at_unix = j.at("ended_at_unix").get<int64_t>();
        summary.status = j.at("status").get<std::string>();
        summary.error = j.value("error", "");
        if (j.contains("perturbation_types")) {
            summary.perturbation_types = j.at("perturbation_types").get<std::vector<std::string>>();
        }
        summary.normal_end_t = j.at("normal_end_t").get<double>();
        summary.chaos_end_t = j.at("chaos_end_t").get<double>();
        summary.run_result.passed = j.at("passed").get<bool>();
        summary.run_result.manifest_name = j.at("manifest_name").get<std::string>();
        summary.run_result.target_id = j.at("target_id").get<std::string>();
        summary.run_result.duration_s = j.at("duration_s").get<double>();
        summary.run_result.started_at = j.at("started_at").get<std::string>();
        if (j.contains("results") && j.at("results").is_array()) {
            for (const auto& r : j.at("results")) {
                validation::ValidationResult vr;
                vr.passed = r.at("passed").get<bool>();
                vr.expectation_type = r.at("type").get<std::string>();
                vr.message = r.value("message", "");
                summary.run_result.results.push_back(std::move(vr));
            }
        }
        return summary;
    } catch (const nlohmann::json::exception& e) {
        SPDLOG_WARN("Failed to parse run summary: {}", e.what());
        return std::nullopt;
    }
}

std::optional<RunRecord> runRecordFromJson(const nlohmann::json& j) {
    try {
        RunRecord record;
        if (!j.contains("summary") || !j.contains("manifest")) {
            return std::nullopt;
        }
        auto summary_opt = runSummaryFromJson(j.at("summary"));
        if (!summary_opt.has_value()) {
            return std::nullopt;
        }
        record.summary = std::move(*summary_opt);
        const auto& m = j.at("manifest");
        record.manifest.test_name = m.at("test_name").get<std::string>();
        record.manifest.target.id = m.at("target_id").get<std::string>();
        if (m.contains("duration_s") && !m.at("duration_s").is_null()) {
            record.manifest.duration_s = m.at("duration_s").get<uint32_t>();
        }
        if (m.contains("perturbations") && m.at("perturbations").is_array()) {
            for (const auto& p : m.at("perturbations")) {
                manifests::Perturbation pert;
                pert.type = p.at("type").get<std::string>();
                if (p.contains("parameters")) {
                    pert.parameters = p.at("parameters").get<manifests::Parameters>();
                }
                record.manifest.perturbations.push_back(std::move(pert));
            }
        }
        if (m.contains("expectations") && m.at("expectations").is_array()) {
            for (const auto& e : m.at("expectations")) {
                manifests::Expectation exp;
                exp.type = e.at("type").get<std::string>();
                if (e.contains("parameters")) {
                    exp.parameters = e.at("parameters").get<manifests::Parameters>();
                }
                exp.continuous = e.value("continuous", false);
                record.manifest.expectations.push_back(std::move(exp));
            }
        }
        if (j.contains("samples") && j.at("samples").is_array()) {
            for (const auto& s : j.at("samples")) {
                RunSample sample;
                sample.t = s.at("t").get<double>();
                sample.phase = s.value("phase", "");
                if (s.contains("cpu_usage_percent")) {
                    sample.cpu_usage_percent = s.at("cpu_usage_percent").get<double>();
                }
                if (s.contains("memory_usage_mb")) {
                    sample.memory_usage_mb = s.at("memory_usage_mb").get<double>();
                }
                if (s.contains("network_rx_bps")) {
                    sample.network_rx_bps = s.at("network_rx_bps").get<double>();
                }
                if (s.contains("network_tx_bps")) {
                    sample.network_tx_bps = s.at("network_tx_bps").get<double>();
                }
                if (s.contains("network_latency_ms")) {
                    sample.network_latency_ms = s.at("network_latency_ms").get<double>();
                }
                record.samples.push_back(std::move(sample));
            }
        }
        if (j.contains("logs") && j.at("logs").is_array()) {
            record.logs = j.at("logs").get<std::vector<std::string>>();
        }
        return record;
    } catch (const nlohmann::json::exception& e) {
        SPDLOG_WARN("Failed to parse run record: {}", e.what());
        return std::nullopt;
    }
}

}  // namespace chaos::orchestrator::history
