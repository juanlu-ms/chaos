#pragma once

#include <cstdint>
#include <string_view>
namespace chaos::orchestrator::containers {

/**
 * @brief Possible states of a container.
 */
enum class ContainerStatus : uint8_t {
    Running,
    Exited,
    Paused,
    Dead,
    Unknown,
};

// From string to enum
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

// From enum to string
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
