#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace chaos::orchestrator::manifests {

struct TransparentStringHash {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(std::string_view key) const noexcept {
        return std::hash<std::string_view>{}(key);
    }

    [[nodiscard]] std::size_t operator()(const std::string& key) const noexcept {
        return (*this)(std::string_view{key});
    }

    [[nodiscard]] std::size_t operator()(const char* key) const noexcept { return (*this)(std::string_view{key}); }
};

using Parameters = std::unordered_map<std::string, std::string, TransparentStringHash, std::equal_to<>>;

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
