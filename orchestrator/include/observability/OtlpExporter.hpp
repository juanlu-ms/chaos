#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace chaos::orchestrator::observability {

using json = nlohmann::json;

/**
 * Minimal OTLP Logs exporter using JSON over HTTP.
 *
 * Builds payloads conforming to the OTLP Logs protocol (JSON variant)
 * and sends them to a configurable OTLP HTTP endpoint.
 */
class OtlpExporter {
public:
    explicit OtlpExporter(std::string endpoint);

    /**
     * Build an OTLP Logs payload for a CHAOS event.
     *
     * @param run_id      Unique run identifier.
     * @param severity    Severity/event type (e.g. "run_started", "state_changed", "run_complete").
     * @param body_json   Freeform JSON body describing the event.
     * @param attributes  Key-value resource attributes (e.g. service.name).
     * @return JSON payload conforming to OTLP Logs protocol.
     */
    static json buildLogPayload(const std::string& run_id, const std::string& severity, const std::string& body_json,
                                const std::unordered_map<std::string, std::string>& attributes);

    /**
     * Export a log payload to the OTLP endpoint.
     * @return true on HTTP 200.
     */
    bool exportLogs(const json& payload);

    /** @return the full OTLP Logs URL. */
    std::string endpoint() const;

private:
    std::string endpoint_;
};

}  // namespace chaos::orchestrator::observability
