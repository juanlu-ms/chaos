#pragma once

#include <gmock/gmock.h>

#include <vector>

#include "history/IRunHistory.hpp"
#include "history/RunRecord.hpp"

namespace chaos::orchestrator::tests {

class MockRunHistory : public history::IRunHistory {
public:
    MOCK_METHOD(void, save, (const history::RunRecord&), (override));
    MOCK_METHOD(std::vector<history::RunSummary>, list, (), (override));
    MOCK_METHOD(std::optional<history::RunRecord>, get, (const std::string&), (override));
    MOCK_METHOD(bool, remove, (const std::string&), (override));
    MOCK_METHOD(void, clear, (), (override));
};

}  // namespace chaos::orchestrator::tests
