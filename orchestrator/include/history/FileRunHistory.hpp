/**
 * @file FileRunHistory.hpp
 * @brief File-system-backed run history storage.
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "history/IRunHistory.hpp"
#include "history/RunRecord.hpp"

namespace chaos::orchestrator::history {

/**
 * @brief File-system-backed run history storage.
 *
 * Stores each run as <dir>/<id>.json. Maintains an in-memory summary cache
 * for fast listing. Retention capped at max_runs (oldest by started_at_unix
 * pruned on each save). Thread-safe via internal shared_mutex (concurrent
 * readers, exclusive writers).
 */
class FileRunHistory : public IRunHistory {
public:
    /**
     * @brief Construct with storage directory and retention cap.
     * @param dir Directory for run JSON files. Created if missing.
     * @param max_runs Maximum number of runs to retain.
     */
    explicit FileRunHistory(std::filesystem::path dir, size_t max_runs = 50);

    /**
     * @brief Persist a run record to a JSON file on disk.
     * @param record The run record to save.
     */
    void save(const RunRecord& record) override;

    /**
     * @brief List all run summaries from the in-memory cache.
     * @return A vector of RunSummary objects.
     */
    std::vector<RunSummary> list() override;

    /**
     * @brief Load a full run record by its ID.
     * @param id The run identifier.
     * @return The RunRecord if found, std::nullopt otherwise.
     */
    std::optional<RunRecord> get(const std::string& id) override;

    /**
     * @brief Delete a run record by its ID.
     * @param id The run identifier.
     * @return true if the record was found and removed, false otherwise.
     */
    bool remove(const std::string& id) override;

    /**
     * @brief Delete all run records from the storage directory.
     */
    void clear() override;

private:
    /**
     * @brief Re-scan the storage directory and rebuild the summary cache.
     */
    void refreshCache();

    /**
     * @brief Delete oldest runs until the cache size respects max_runs_.
     */
    void prune();

    /**
     * @brief Check whether a file path matches the run file naming convention.
     */
    static bool isValidRunFile(const std::filesystem::path& path);

    std::filesystem::path dir_;
    size_t max_runs_;
    std::vector<RunSummary> cache_;
    mutable std::shared_mutex mutex_;
};

}  // namespace chaos::orchestrator::history
