#pragma once

#include <gmock/gmock.h>

#include "containers/IContainerEngine.hpp"
#include <string_view>
#include <vector>

/**
 * @file MockContainerEngine.hpp
 * @brief Google Mock test double for the container engine port.
 */

namespace chaos::orchestrator::tests {

/**
 * @brief Mock implementation of IContainerEngine used by perturbation tests.
 */
class MockContainerEngine : public containers::IContainerEngine {
public:
    MOCK_METHOD(std::vector<containers::Container>, listContainers, (), (const, override));
    MOCK_METHOD(void, pullImage, (const std::string_view image), (const, override));
    MOCK_METHOD(void, buildImage, (const std::string_view imageName, const std::string_view dockerfilePath),
                (const, override));
    MOCK_METHOD(std::string, createContainer, (const std::string_view image, const std::vector<std::string>& options),
                (const, override));
    MOCK_METHOD(void, startContainer, (const std::string_view containerId), (const, override));
    MOCK_METHOD(void, stopContainer, (const std::string_view containerId), (const, override));
    MOCK_METHOD(void, killContainer, (const std::string_view containerId), (const, override));
    MOCK_METHOD(void, removeContainer, (const std::string_view containerId), (const, override));
    MOCK_METHOD(std::string, exec, (const std::string_view containerId, const std::string_view command),
                (const, override));
    MOCK_METHOD(shared::ContainerStatus, getStatus, (const std::string_view containerId), (const, override));
    MOCK_METHOD(std::string, getLogs, (const std::string_view containerId), (const, override));
    MOCK_METHOD(void, updateMemoryLimit, (const std::string_view containerId, int64_t memory_bytes), (const, override));
    MOCK_METHOD(void, updateCpuQuota, (const std::string_view containerId, int64_t cpu_quota, int64_t cpu_period),
                (const, override));
    MOCK_METHOD(double, getContainerMemoryUsage, (const std::string_view containerId), (const, override));
    MOCK_METHOD(double, getContainerCpuUsage, (const std::string_view containerId), (const, override));
    MOCK_METHOD(std::string, getContainerIp, (const std::string_view containerId), (const, override));
    MOCK_METHOD(containers::SystemInfo, getSystemInfo, (), (const, override));
};

}  // namespace chaos::orchestrator::tests
