#include "history/FileRunHistory.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <shared_mutex>

#include "history/HistoryJson.hpp"
#include "history/RunRecord.hpp"

namespace chaos::orchestrator::history {

FileRunHistory::FileRunHistory(std::filesystem::path dir, size_t max_runs) : dir_(std::move(dir)), max_runs_(max_runs) {
    std::filesystem::create_directories(dir_);
}

void FileRunHistory::save(const RunRecord& record) {
    std::unique_lock lock(mutex_);

    auto j = runRecordToJson(record);
    auto filepath = dir_ / (record.summary.id + ".json");
    {
        std::ofstream file(filepath);
        file << j.dump();
    }

    refreshCache();
    prune();
}

std::vector<RunSummary> FileRunHistory::list() {
    std::unique_lock lock(mutex_);

    if (cache_.empty()) {
        refreshCache();
    }
    return cache_;
}

std::optional<RunRecord> FileRunHistory::get(const std::string& id) {
    std::shared_lock lock(mutex_);

    auto filepath = dir_ / (id + ".json");
    if (!std::filesystem::exists(filepath)) {
        return std::nullopt;
    }

    std::ifstream file(filepath);
    std::string content{std::istreambuf_iterator<char>(file), {}};
    return runRecordFromJson(nlohmann::json::parse(content));
}

bool FileRunHistory::remove(const std::string& id) {
    std::unique_lock lock(mutex_);

    auto filepath = dir_ / (id + ".json");
    bool existed = std::filesystem::exists(filepath);
    if (!existed) {
        return false;
    }

    std::filesystem::remove(filepath);

    auto iter = std::ranges::find_if(cache_, [&id](const RunSummary& sum) { return sum.id == id; });
    if (iter != cache_.end()) {
        cache_.erase(iter);
    }
    return true;
}

void FileRunHistory::clear() {
    std::unique_lock lock(mutex_);

    for (const auto& summary : cache_) {
        std::filesystem::remove(dir_ / (summary.id + ".json"));
    }
    cache_.clear();
}

void FileRunHistory::refreshCache() {
    cache_.clear();

    for (const auto& entry : std::filesystem::directory_iterator(dir_)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!isValidRunFile(entry.path())) {
            continue;
        }

        std::ifstream file(entry.path());
        std::string content{std::istreambuf_iterator<char>(file), {}};

        try {
            auto j = nlohmann::json::parse(content);
            auto summary = runSummaryFromJson(j.at("summary"));
            if (summary.has_value()) {
                cache_.push_back(std::move(*summary));
            }
        } catch (const nlohmann::json::exception& e) {
            SPDLOG_WARN("skipping corrupt history file: {}", e.what());
            continue;
        }
    }

    std::ranges::sort(cache_, std::greater{}, &RunSummary::started_at_unix);
}

void FileRunHistory::prune() {
    while (cache_.size() > max_runs_) {
        const auto& oldest = cache_.back();
        std::filesystem::remove(dir_ / (oldest.id + ".json"));
        cache_.pop_back();
    }
}

bool FileRunHistory::isValidRunFile(const std::filesystem::path& path) {
    auto filename = path.filename().string();
    return filename.starts_with("run-") && filename.ends_with(".json");
}

}  // namespace chaos::orchestrator::history
