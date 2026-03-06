#pragma once
#include <string>

namespace chaos::domain {

/**
 * @brief Represents a Docker container summary.
 */
struct Container {
    /** @brief Docker container ID. */
    std::string id;
    /** @brief Human-friendly container name without leading '/'. */
    std::string name;
    /** @brief Current container state (e.g., running, exited). */
    std::string state;
};

}  // namespace chaos::domain
