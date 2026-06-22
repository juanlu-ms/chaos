/**
 * @file ContainerStatus.hpp
 * @brief Enumeration of container lifecycle states and associated conversion
 * functions.
 */

#pragma once

#include <cstdint>
#include <string_view>
namespace chaos::orchestrator::containers {

/**
 * @brief Possible states of a container.
 */
enum class ContainerStatus : uint8_t {
    /** @brief Container is currently running. */
    Running,
    /** @brief Container has exited (stopped normally). */
    Exited,
    /** @brief Container is paused. */
    Paused,
    /** @brief Container is in a dead state and cannot be restarted. */
    Dead,
    /** @brief Container status could not be determined. */
    Unknown,
};

/**
 * @brief Parse a container status string into its enum representation.
 * @param status_str The status string (e.g. "running", "exited").
 * @return The corresponding ContainerStatus enum value, or Unknown if unrecognised.
 */
inline ContainerStatus parseContainerStatus(std::string_view status_str) {
    using enum chaos::orchestrator::containers::ContainerStatus;
    if (status_str == "running") {
        return Running;
    }
    if (status_str == "exited") {
        return Exited;
    }
    if (status_str == "paused") {
        return Paused;
    }
    if (status_str == "dead") {
        return Dead;
    }
    return Unknown;
}

/**
 * @brief Convert a ContainerStatus enum value to its string representation.
 * @param status The ContainerStatus enum value.
 * @return The corresponding string (e.g. "running", "unknown").
 */
constexpr std::string_view toString(ContainerStatus status) {
    switch (status) {
        using enum chaos::orchestrator::containers::ContainerStatus;
        case Running:
            return "running";
        case Exited:
            return "exited";
        case Paused:
            return "paused";
        case Dead:
            return "dead";
        default:
            return "unknown";
    }
}

}  // namespace chaos::orchestrator::containers
