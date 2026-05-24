#include "manifests/ManifestParser.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::manifests {

namespace {

struct RequiredParameterRule {
    std::string parameter;
    std::string errorMessage;
};

using StringSet = std::unordered_set<std::string, TransparentStringHash, std::equal_to<>>;
using RuleMap =
    std::unordered_map<std::string, std::vector<RequiredParameterRule>, TransparentStringHash, std::equal_to<>>;

const StringSet kSupportedPerturbations = {"kill",          "memory_cap",     "cpu_cap",
                                           "network_delay", "network_cutoff", "garbage_packet"};

const RuleMap kPerturbationRequiredParams = {
    {"memory_cap", {{"limit_bytes", "Missing 'limit_bytes' parameter for memory_cap perturbation"}}},
    {"cpu_cap", {{"cpu_cores", "Missing 'cpu_cores' parameter for cpu_cap perturbation"}}},
    {"network_delay", {{"delay_ms", "Missing 'delay_ms' parameter for network_delay perturbation"}}}};

const StringSet kSupportedExpectations = {"container_running", "container_not_running", "log_contains",
                                          "log_not_contains",  "http_status",           "http_latency"};

const RuleMap kExpectationRequiredParams = {
    {"log_contains", {{"substring", "Missing 'substring' parameter for log expectation"}}},
    {"log_not_contains", {{"substring", "Missing 'substring' parameter for log expectation"}}},
    {"http_status",
     {{"port", "Missing 'port' parameter for http_status expectation"},
      {"path", "Missing 'path' parameter for http_status expectation"},
      {"expected_status", "Missing 'expected_status' parameter for http_status expectation"}}},
    {"http_latency",
     {{"port", "Missing 'port' parameter for http_latency expectation"},
      {"path", "Missing 'path' parameter for http_latency expectation"},
      {"max_latency_ms", "Missing 'max_latency_ms' parameter for http_latency expectation"}}}};

void validateRequiredParameters(const std::string& type, const nlohmann::json& parameters, const RuleMap& rules) {
    const auto iter = rules.find(type);
    if (iter == rules.end()) {
        return;
    }

    for (const auto& rule : iter->second) {
        if (!parameters.contains(rule.parameter)) {
            throw ManifestParserError(rule.errorMessage);
        }
    }
}

}  // namespace

void from_json(const nlohmann::json& j, Target& t) {
    if (j.contains("id")) {
        j.at("id").get_to(t.id);
    } else if (j.contains("name")) {
        j.at("name").get_to(t.id);
    } else {
        throw ManifestParserError("Target must define either 'id' or 'name'");
    }
}

void from_json(const nlohmann::json& j, Perturbation& p) {
    j.at("type").get_to(p.type);
    if (!kSupportedPerturbations.contains(p.type)) {
        throw ManifestParserError("Unsupported perturbation type: " + p.type);
    }

    if (j.contains("parameters")) {
        j.at("parameters").get_to(p.parameters);
    }

    validateRequiredParameters(p.type, p.parameters, kPerturbationRequiredParams);
}

void from_json(const nlohmann::json& j, Expectation& e) {
    j.at("type").get_to(e.type);
    if (!kSupportedExpectations.contains(e.type)) {
        throw ManifestParserError("Unsupported expectation type: " + e.type);
    }

    if (j.contains("parameters")) {
        j.at("parameters").get_to(e.parameters);
    }

    if (j.contains("continuous")) {
        j.at("continuous").get_to(e.continuous);
    } else {
        static const StringSet kContinuousTypes = {"container_running", "log_contains", "log_not_contains"};
        e.continuous = kContinuousTypes.contains(e.type);
    }

    validateRequiredParameters(e.type, e.parameters, kExpectationRequiredParams);
}

void from_json(const nlohmann::json& j, ChaosManifest& m) {
    j.at("test_name").get_to(m.test_name);
    j.at("target").get_to(m.target);
    j.at("perturbations").get_to(m.perturbations);
    if (j.contains("expectations")) {
        j.at("expectations").get_to(m.expectations);
    }
    if (j.contains("duration_s")) {
        uint32_t val;
        j.at("duration_s").get_to(val);
        m.duration_s = val;
    }
}

ChaosManifest ManifestParser::parseFromFile(const std::string& filepath) {
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
