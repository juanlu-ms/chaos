/**
 * @file FileRunHistory.hpp
 * @brief File-system-backed run history storage.
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "history/IRunHistory.hpp"
#include "history/RunRecord.hpp"

namespace chaos::orchestrator::history {

/**
 * @brief File-system-backed run history storage.
 *
 * Stores each run as <dir>/<id>.json. Maintains an in-memory summary cache
 * for fast listing. Retention capped at maxRuns (oldest by started_at_unix
 * pruned on each save). Thread-safe via internal mutex.
 */
class FileRunHistory : public IRunHistory {
public:
    /**
     * @brief Construct with storage directory and retention cap.
     * @param dir Directory for run JSON files. Created if missing.
     * @param maxRuns Maximum number of runs to retain.
     */
    explicit FileRunHistory(std::filesystem::path dir, size_t maxRuns = 50);

    void save(const RunRecord& record) override;
    std::vector<RunSummary> list() override;
    std::optional<RunRecord> get(const std::string& id) override;
    bool remove(const std::string& id) override;
    void clear() override;

private:
    /**
     * @brief Re-scan the storage directory and rebuild the summary cache.
     */
    void refreshCache();

    /**
     * @brief Delete oldest runs until the cache size respects maxRuns_.
     */
    void prune();

    /**
     * @brief Check whether a file path matches the run file naming convention.
     */
    static bool isValidRunFile(const std::filesystem::path& path);

    std::filesystem::path dir_;
    size_t maxRuns_;
    std::vector<RunSummary> cache_;
    mutable std::mutex mutex_;
};

}  // namespace chaos::orchestrator::history
