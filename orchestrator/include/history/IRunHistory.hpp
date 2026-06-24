/**
 * @file IRunHistory.hpp
 * @brief Facade interface for persisting and querying completed run records.
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace chaos::orchestrator::history {

struct RunRecord;
struct RunSummary;

/**
 * @brief Facade interface for persisting and querying completed run records.
 */
class IRunHistory {
public:
    /**
     * @brief Persist a run record to the backing store.
     * @param record The complete run record to save.
     */
    virtual void save(const RunRecord& record) = 0;

    /**
     * @brief List all saved run summaries, sorted by started_at_unix descending.
     * @return A vector of RunSummary objects from the backing store.
     */
    virtual std::vector<RunSummary> list() = 0;

    /**
     * @brief Retrieve a full run record by its unique identifier.
     * @param id The run identifier (e.g., "run-1719000000000").
     * @return The matching RunRecord, or std::nullopt if not found.
     */
    virtual std::optional<RunRecord> get(const std::string& id) = 0;

    /**
     * @brief Remove a run record from the backing store.
     * @param id The run identifier to remove.
     * @return true if the record was found and removed, false otherwise.
     */
    virtual bool remove(const std::string& id) = 0;

    /**
     * @brief Remove all run records from the backing store.
     */
    virtual void clear() = 0;

    /**
     * @brief Virtual destructor (defaulted).
     */
    virtual ~IRunHistory() = default;
};

}  // namespace chaos::orchestrator::history
