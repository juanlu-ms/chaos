/**
 * @file Manifest.hpp
 * @brief Data structures for chaos testing manifests.
 */

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <optional>
#include <cstdint>

namespace chaos::orchestrator::manifests {

/** @brief Helper struct for transparent string hashing. */
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

/** @brief Parameters map with heterogeneous lookup support. */
using Parameters = std::unordered_map<std::string, std::string, TransparentStringHash, std::equal_to<>>;

/** @brief Identifies the target container for the test. */
struct Target {
    /** @brief Target identifier (e.g., container ID/name). */
    std::string id;
};

/** @brief Defines a perturbation to be applied. */
struct Perturbation {
    /** @brief Type of perturbation (e.g., 'network-delay'). */
    std::string type;
    /** @brief Parameters configuring the perturbation. */
    Parameters parameters;
};

/** @brief Defines an expectation to validate the system. */
struct Expectation {
    /** @brief Type of expectation (e.g., 'log-contains'). */
    std::string type;
    /** @brief Parameters configuring the expectation. */
    Parameters parameters;
};

/** @brief Root structure for a chaos test manifest. */
struct ChaosManifest {
    /** @brief Name of the chaos test. */
    std::string test_name;
    /** @brief Target of the attack. */
    Target target;
    /** @brief Sequence of perturbations to apply. */
    std::vector<Perturbation> perturbations;
    /** @brief Sequence of expectations to validate. */
    std::vector<Expectation> expectations;
    /** @brief Optional duration limit for the test in seconds. */
    std::optional<uint32_t> duration_s;
};

}  // namespace chaos::orchestrator::manifests
