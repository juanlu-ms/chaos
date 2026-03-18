#pragma once

#include <gmock/gmock.h>
#include <containers/IContainerEngine.hpp>
#include <string_view>
#include <vector>

namespace chaos::orchestrator::tests {

class MockContainerEngine : public chaos::orchestrator::containers::IContainerEngine {
public:
    MOCK_METHOD(std::vector<chaos::orchestrator::containers::Container>, listContainers, (), (override));
    MOCK_METHOD(void, stopContainer, (const std::string_view containerId), (override));
    MOCK_METHOD(void, killContainer, (const std::string_view containerId), (override));
};

} // namespace chaos::orchestrator::tests
