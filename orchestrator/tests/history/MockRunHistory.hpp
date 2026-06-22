/**
 * @file MockRunHistory.hpp
 * @brief Google Mock test double for the run history persistence port.
 */

#pragma once

#include <gmock/gmock.h>

#include <vector>

#include "history/IRunHistory.hpp"
#include "history/RunRecord.hpp"

namespace chaos::orchestrator::tests {

/**
 * @brief Mock implementation of IRunHistory used by history-related tests.
 */
class MockRunHistory : public history::IRunHistory {
public:
    /** @brief Persists a run record. @param record The run record to save. */
    MOCK_METHOD(void, save, (const history::RunRecord&), (override));
    /** @brief Lists all saved run summaries. @return A vector of run summaries. */
    MOCK_METHOD(std::vector<history::RunSummary>, list, (), (override));
    /** @brief Retrieves a run record by identifier. @param id The unique run identifier. @return The run record if
     * found, or std::nullopt. */
    MOCK_METHOD(std::optional<history::RunRecord>, get, (const std::string&), (override));
    /** @brief Removes a run record by identifier. @param id The unique run identifier. @return true if removed, false
     * if not found. */
    MOCK_METHOD(bool, remove, (const std::string&), (override));
    /** @brief Removes all persisted run records. */
    MOCK_METHOD(void, clear, (), (override));
};

}  // namespace chaos::orchestrator::tests
