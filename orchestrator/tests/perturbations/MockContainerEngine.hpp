/**
 * @file MockContainerEngine.hpp
 * @brief Google Mock test double for the container engine port.
 */

#pragma once

#include <gmock/gmock.h>

#include <string_view>
#include <vector>

#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::tests {

/**
 * @brief Mock implementation of IContainerEngine used by perturbation tests.
 */
class MockContainerEngine : public containers::IContainerEngine {
public:
    /** @brief Lists all containers known to the engine. @return A vector of Container objects. */
    MOCK_METHOD(std::vector<containers::Container>, listContainers, (), (const, override));
    /** @brief Pulls a container image from a registry. @param image The image name (optionally with tag). */
    MOCK_METHOD(void, pullImage, (const std::string_view image), (const, override));
    /** @brief Builds a container image from a Dockerfile. @param imageName The name to assign to the built image.
     * @param dockerfilePath Path to the Dockerfile. */
    MOCK_METHOD(void, buildImage, (const std::string_view imageName, const std::string_view dockerfilePath),
                (const, override));
    /** @brief Creates a container from an image. @param image The image to use. @param options Additional creation
     * options. @return The new container's identifier. */
    MOCK_METHOD(std::string, createContainer, (const std::string_view image, const std::vector<std::string>& options),
                (const, override));
    /** @brief Starts a previously created container. @param containerId The container identifier. */
    MOCK_METHOD(void, startContainer, (const std::string_view containerId), (const, override));
    /** @brief Stops a running container gracefully. @param containerId The container identifier. */
    MOCK_METHOD(void, stopContainer, (const std::string_view containerId), (const, override));
    /** @brief Force-kills a container. @param containerId The container identifier. */
    MOCK_METHOD(void, killContainer, (const std::string_view containerId), (const, override));
    /** @brief Removes a stopped container. @param containerId The container identifier. */
    MOCK_METHOD(void, removeContainer, (const std::string_view containerId), (const, override));
    /** @brief Executes a command inside a container. @param containerId The container identifier. @param command The
     * command string to run. @return The command's standard output. */
    MOCK_METHOD(std::string, exec, (const std::string_view containerId, const std::string_view command),
                (const, override));
    /** @brief Executes a command inside a container's network namespace. @param containerId The container identifier.
     * @param command The command string to run. @return The command's standard output. */
    MOCK_METHOD(std::string, execInNetNs, (const std::string_view containerId, const std::string_view command),
                (const, override));
    /** @brief Queries the current status of a container. @param containerId The container identifier. @return The
     * container's status. */
    MOCK_METHOD(containers::ContainerStatus, getStatus, (const std::string_view containerId), (const, override));
    /** @brief Retrieves the logs of a container. @param containerId The container identifier. @return The container's
     * log output. */
    MOCK_METHOD(std::string, getLogs, (const std::string_view containerId), (const, override));
    /** @brief Updates the memory limit for a container. @param containerId The container identifier. @param
     * memory_bytes The new memory limit in bytes. */
    MOCK_METHOD(void, updateMemoryLimit, (const std::string_view containerId, int64_t memory_bytes), (const, override));
    /** @brief Updates the CPU quota and period for a container. @param containerId The container identifier. @param
     * cpu_quota The CPU quota in microseconds. @param cpu_period The CPU period in microseconds. */
    MOCK_METHOD(void, updateCpuQuota, (const std::string_view containerId, int64_t cpu_quota, int64_t cpu_period),
                (const, override));
    /** @brief Retrieves resource usage statistics for a container. @param containerId The container identifier. @return
     * The container's statistics. */
    MOCK_METHOD(containers::ContainerStats, getStats, (const std::string_view containerId), (override));
    /** @brief Gets the IP address of a container. @param containerId The container identifier. @return The container's
     * IP address. */
    MOCK_METHOD(std::string, getContainerIp, (const std::string_view containerId), (const, override));
    /** @brief Gets the network namespace file descriptor of a container. @param containerId The container identifier.
     * @return The network namespace file descriptor. */
    MOCK_METHOD(int, getContainerNetnsFd, (const std::string_view containerId), (const, override));
    /** @brief Gets the PID of a container's init process. @param containerId The container identifier. @return The
     * container's PID. */
    MOCK_METHOD(int, getContainerPid, (const std::string_view containerId), (const, override));
    /** @brief Queries system-wide information from the container runtime. @return System information including OS,
     * architecture, and container count. */
    MOCK_METHOD(containers::SystemInfo, getSystemInfo, (), (const, override));
};

}  // namespace chaos::orchestrator::tests
