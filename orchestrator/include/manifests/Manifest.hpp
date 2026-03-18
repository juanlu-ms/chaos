#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace chaos::orchestrator::manifests {

using Parameters = std::unordered_map<std::string, std::string>;

struct Target {
    std::string type;
    std::string name;
};

struct Perturbation {
    std::string type;
    Parameters parameters;
};

struct Expectation {
    std::string type;
    Parameters parameters;
};

struct ChaosManifest {
    std::string test_name;
    Target target;
    std::vector<Perturbation> perturbations;
    std::vector<Expectation> expectations;
};

}  // namespace chaos::orchestrator::manifests
