#include "history/FileRunHistory.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>

#include "history/HistoryJson.hpp"
#include "history/RunRecord.hpp"

namespace chaos::orchestrator::history {

FileRunHistory::FileRunHistory(std::filesystem::path dir, size_t maxRuns) : dir_(std::move(dir)), maxRuns_(maxRuns) {
    std::filesystem::create_directories(dir_);
}

void FileRunHistory::save(const RunRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

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
    std::lock_guard<std::mutex> lock(mutex_);

    if (cache_.empty()) {
        refreshCache();
    }
    return cache_;
}

std::optional<RunRecord> FileRunHistory::get(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto filepath = dir_ / (id + ".json");
    if (!std::filesystem::exists(filepath)) {
        return std::nullopt;
    }

    std::ifstream file(filepath);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return runRecordFromJson(nlohmann::json::parse(content));
}

bool FileRunHistory::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto filepath = dir_ / (id + ".json");
    bool existed = std::filesystem::exists(filepath);
    if (!existed) {
        return false;
    }

    std::filesystem::remove(filepath);

    auto iter = std::find_if(cache_.begin(), cache_.end(), [&id](const RunSummary& sum) { return sum.id == id; });
    if (iter != cache_.end()) {
        cache_.erase(iter);
    }
    return true;
}

void FileRunHistory::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

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
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        try {
            auto j = nlohmann::json::parse(content);
            auto summary = runSummaryFromJson(j.at("summary"));
            if (summary.has_value()) {
                cache_.push_back(std::move(*summary));
            }
        } catch (...) {
            continue;
        }
    }

    std::sort(cache_.begin(), cache_.end(),
              [](const RunSummary& lhs, const RunSummary& rhs) { return lhs.started_at_unix > rhs.started_at_unix; });
}

void FileRunHistory::prune() {
    while (cache_.size() > maxRuns_) {
        auto& oldest = cache_.back();
        std::filesystem::remove(dir_ / (oldest.id + ".json"));
        cache_.pop_back();
    }
}

bool FileRunHistory::isValidRunFile(const std::filesystem::path& path) {
    auto filename = path.filename().string();
    return filename.starts_with("run-") && filename.ends_with(".json");
}

}  // namespace chaos::orchestrator::history
