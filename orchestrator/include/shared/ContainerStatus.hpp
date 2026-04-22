#pragma once

#include <string_view>
namespace chaos::orchestrator::shared {

/**
 * @brief Possible states of a container.
 */
enum class ContainerStatus { Running, Exited, Paused, Dead, NotFound, Unknown };

// From string to enum
inline ContainerStatus parseContainerStatus(std::string_view status_str) {
    using enum chaos::orchestrator::shared::ContainerStatus;
    if (status_str == "running") return Running;
    if (status_str == "exited") return Exited;
    if (status_str == "paused") return Paused;
    if (status_str == "dead") return Dead;
    return Unknown;
}

// From enum to string
inline std::string_view toString(ContainerStatus status) {
    switch (status) {
        using enum chaos::orchestrator::shared::ContainerStatus;
        case Running:
            return "running";
        case Exited:
            return "exited";
        case Paused:
            return "paused";
        case Dead:
            return "dead";
        case NotFound:
            return "not_found";
        default:
            return "unknown";
    }
}

}  // namespace chaos::orchestrator::shared
