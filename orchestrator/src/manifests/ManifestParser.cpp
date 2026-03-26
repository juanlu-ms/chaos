#include "manifests/ManifestParser.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::manifests {

void from_json(const nlohmann::json& j, Target& t) { j.at("id").get_to(t.id); }

void from_json(const nlohmann::json& j, Perturbation& p) {
    j.at("type").get_to(p.type);
    if (p.type != "kill" && p.type != "memory_cap" && p.type != "cpu_cap" && p.type != "network_delay" &&
        p.type != "network_cutoff" && p.type != "garbage_packet") {
        throw ManifestParserError("Unsupported perturbation type: " + p.type);
    }
    if (j.contains("parameters")) {
        j.at("parameters").get_to(p.parameters);
    }

    if (p.type == "memory_cap" && !p.parameters.contains("limit_bytes")) {
        throw ManifestParserError("Missing 'limit_bytes' parameter for memory_cap perturbation");
    }

    if (p.type == "cpu_cap") {
        if (!p.parameters.contains("quota")) {
            throw ManifestParserError("Missing 'quota' parameter for cpu_cap perturbation");
        }
        if (!p.parameters.contains("period")) {
            throw ManifestParserError("Missing 'period' parameter for cpu_cap perturbation");
        }
    }

    if (p.type == "network_delay" && !p.parameters.contains("delay_ms")) {
        throw ManifestParserError("Missing 'delay_ms' parameter for network_delay perturbation");
    }
}

void from_json(const nlohmann::json& j, Expectation& e) {
    j.at("type").get_to(e.type);
    if (e.type != "container_running" && e.type != "container_not_running" && e.type != "log_contains" &&
        e.type != "log_not_contains") {
        throw ManifestParserError("Unsupported expectation type: " + e.type);
    }
    if (j.contains("parameters")) {
        j.at("parameters").get_to(e.parameters);
    }
    if ((e.type == "log_contains" || e.type == "log_not_contains") && !e.parameters.contains("substring")) {
        throw ManifestParserError("Missing 'substring' parameter for log expectation");
    }
}

void from_json(const nlohmann::json& j, ChaosManifest& m) {
    j.at("test_name").get_to(m.test_name);
    j.at("target").get_to(m.target);
    j.at("perturbations").get_to(m.perturbations);
    if (j.contains("expectations")) {
        j.at("expectations").get_to(m.expectations);
    }
}

ChaosManifest ManifestParser::parse(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw ManifestParserError("Failed to open manifest file: " + filepath);
    }

    nlohmann::json manifest_json;
    try {
        file >> manifest_json;
    } catch (const nlohmann::json::parse_error& e) {
        throw ManifestParserError("JSON parse error in " + filepath + ": " + e.what());
    }

    try {
        return manifest_json.get<ChaosManifest>();
    } catch (const nlohmann::json::exception& e) {
        throw ManifestParserError("Manifest validation error in " + filepath + ": " + e.what());
    }
}

ChaosManifest ManifestParser::parseFromJson(const std::string& jsonStr) {
    nlohmann::json manifest_json = nlohmann::json::parse(jsonStr, nullptr, false);
    if (manifest_json.is_discarded()) {
        throw ManifestParserError("Invalid JSON string");
    }

    try {
        return manifest_json.get<ChaosManifest>();
    } catch (const nlohmann::json::exception& e) {
        throw ManifestParserError("Manifest validation error from JSON string: " + std::string(e.what()));
    }
}

}  // namespace chaos::orchestrator::manifests
