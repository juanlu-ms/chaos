#include "observability/OtlpExporter.hpp"

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <chrono>

namespace chaos::orchestrator::observability {

OtlpExporter::OtlpExporter(std::string endpoint) : endpoint_(std::move(endpoint)) {}

json OtlpExporter::buildLogPayload(const std::string& run_id, const std::string& severity, const std::string& body_json,
                                   const std::unordered_map<std::string, std::string>& attributes) {
    auto now = std::chrono::system_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    json resourceAttrs = json::array();
    for (const auto& [key, value] : attributes) {
        resourceAttrs.push_back({{"key", key}, {"value", {{"stringValue", value}}}});
    }

    json payload = {
        {"resourceLogs",
         json::array(
             {{{"resource", {{"attributes", std::move(resourceAttrs)}}},
               {"scopeLogs",
                json::array(
                    {{{"scope", json::object()},
                      {"logRecords",
                       json::array({{{"timeUnixNano", std::to_string(nanos)},
                                     {"severityText", severity},
                                     {"body", {{"stringValue", body_json}}},
                                     {"attributes", json::array({{{"key", "run_id"},
                                                                  {"value", {{"stringValue", run_id}}}}})}}})}}})}}})}};

    return payload;
}

bool OtlpExporter::exportLogs(const json& payload) {
    try {
        httplib::Client client(endpoint_);
        client.set_connection_timeout(5);
        client.set_read_timeout(10);
        auto res = client.Post("/v1/logs", payload.dump(), "application/json");
        if (!res) {
            SPDLOG_ERROR("OTLP export request failed: no response");
            return false;
        }
        if (res->status != 200) {
            SPDLOG_ERROR("OTLP export returned HTTP {}", res->status);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("OTLP export exception: {}", e.what());
        return false;
    }
}

std::string OtlpExporter::endpoint() const { return endpoint_ + "/v1/logs"; }

}  // namespace chaos::orchestrator::observability
