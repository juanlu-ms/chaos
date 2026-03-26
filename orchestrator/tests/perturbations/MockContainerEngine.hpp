#pragma once

#include <gmock/gmock.h>

#include <containers/IContainerEngine.hpp>
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
class MockContainerEngine : public chaos::orchestrator::containers::IContainerEngine {
public:
    MOCK_METHOD(std::vector<chaos::orchestrator::containers::Container>, listContainers, (), (override));
    MOCK_METHOD(void, createContainer, (const std::string_view image, const std::vector<std::string>& options),
                (override));
    MOCK_METHOD(void, startContainer, (const std::string_view containerId), (override));
    MOCK_METHOD(void, stopContainer, (const std::string_view containerId), (override));
    MOCK_METHOD(void, killContainer, (const std::string_view containerId), (override));
    MOCK_METHOD(std::string, exec, (const std::string_view containerId, const std::string_view command), (override));
    MOCK_METHOD(std::string, getLogs, (const std::string_view containerId), (override));
    MOCK_METHOD(void, updateResources,
                (const std::string_view containerId, int64_t memory_bytes, int64_t cpu_quota, int64_t cpu_period),
                (override));
};

}  // namespace chaos::orchestrator::tests
