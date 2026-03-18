#include "manifests/ManifestParser.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::manifests {

void from_json(const nlohmann::json& j, Target& t) { j.at("name").get_to(t.name); }

void from_json(const nlohmann::json& j, Perturbation& p) {
    j.at("type").get_to(p.type);
    if (p.type != "kill" && p.type != "memory_cap" && p.type != "cpu_cap" && p.type != "network_cap") {
        throw std::runtime_error("Unsupported perturbation type: " + p.type);
    }
    if (j.contains("parameters")) {
        j.at("parameters").get_to(p.parameters);
    }
}

void from_json(const nlohmann::json& j, Expectation& e) {
    j.at("type").get_to(e.type);
    if (e.type != "container_running" && e.type != "container_not_running" && e.type != "log_contains" &&
        e.type != "log_not_contains") {
        throw std::runtime_error("Unsupported expectation type: " + e.type);
    }
    if (j.contains("parameters")) {
        j.at("parameters").get_to(e.parameters);
    }
    if ((e.type == "log_contains" || e.type == "log_not_contains") &&
        e.parameters.find("substring") == e.parameters.end()) {
        throw std::runtime_error("Missing 'substring' parameter for log expectation");
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
        throw std::runtime_error("Failed to open manifest file: " + filepath);
    }

    nlohmann::json manifest_json;
    try {
        file >> manifest_json;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("JSON parse error in " + filepath + ": " + e.what());
    }

    try {
        return manifest_json.get<ChaosManifest>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Manifest validation error in " + filepath + ": " + e.what());
    }
}

}  // namespace chaos::orchestrator::manifests
