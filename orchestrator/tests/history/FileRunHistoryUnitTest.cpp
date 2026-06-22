/**
 * @file FileRunHistoryUnitTest.cpp
 * @brief Unit tests for FileRunHistory file-system-backed run history storage.
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <thread>

#include "history/FileRunHistory.hpp"
#include "history/RunRecord.hpp"

using namespace chaos::orchestrator::history;

class FileRunHistoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir_ = std::filesystem::temp_directory_path() / ("chaos_test_history_" + std::to_string(std::rand()));
        std::filesystem::create_directories(tempDir_);
    }
    void TearDown() override { std::filesystem::remove_all(tempDir_); }
    std::filesystem::path tempDir_;
};

static RunRecord makeRecord(const std::string& id, int64_t started_at) {
    RunRecord r;
    r.summary.id = id;
    r.summary.started_at_unix = started_at;
    r.summary.status = "completed";
    return r;
}

TEST_F(FileRunHistoryTest, SaveAndList) {
    FileRunHistory history(tempDir_);
    auto record = makeRecord("run-1000000", 1000000);
    history.save(record);

    auto results = history.list();
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, "run-1000000");
}

TEST_F(FileRunHistoryTest, SaveAndGet) {
    FileRunHistory history(tempDir_);
    auto record = makeRecord("run-1000000", 1000000);
    record.summary.ended_at_unix = 1001000;
    record.logs = {"log line"};
    history.save(record);

    auto retrieved = history.get("run-1000000");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->summary.id, "run-1000000");
    EXPECT_EQ(retrieved->summary.started_at_unix, 1000000);
    EXPECT_EQ(retrieved->summary.ended_at_unix, 1001000);
    EXPECT_EQ(retrieved->summary.status, "completed");
    ASSERT_EQ(retrieved->logs.size(), 1u);
    EXPECT_EQ(retrieved->logs[0], "log line");
}

TEST_F(FileRunHistoryTest, SaveMultipleAndListSorted) {
    FileRunHistory history(tempDir_);
    history.save(makeRecord("run-1000", 1000));
    history.save(makeRecord("run-3000", 3000));
    history.save(makeRecord("run-2000", 2000));

    auto results = history.list();
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].started_at_unix, 3000);
    EXPECT_EQ(results[1].started_at_unix, 2000);
    EXPECT_EQ(results[2].started_at_unix, 1000);
}

TEST_F(FileRunHistoryTest, Remove) {
    FileRunHistory history(tempDir_);
    history.save(makeRecord("run-1000000", 1000000));
    history.save(makeRecord("run-2000000", 2000000));

    bool removed = history.remove("run-1000000");
    EXPECT_TRUE(removed);

    auto results = history.list();
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, "run-2000000");
}

TEST_F(FileRunHistoryTest, RemoveNonexistentReturnsFalse) {
    FileRunHistory history(tempDir_);
    bool removed = history.remove("run-nonexistent");
    EXPECT_FALSE(removed);
}

TEST_F(FileRunHistoryTest, Clear) {
    FileRunHistory history(tempDir_);
    history.save(makeRecord("run-1000000", 1000000));
    history.save(makeRecord("run-2000000", 2000000));

    history.clear();
    auto results = history.list();
    EXPECT_TRUE(results.empty());
}

TEST_F(FileRunHistoryTest, RetentionPruning) {
    FileRunHistory history(tempDir_, 2);
    history.save(makeRecord("run-1000", 1000));
    history.save(makeRecord("run-2000", 2000));
    history.save(makeRecord("run-3000", 3000));

    auto results = history.list();
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].started_at_unix, 3000);
    EXPECT_EQ(results[1].started_at_unix, 2000);
}

TEST_F(FileRunHistoryTest, DirAutoCreate) {
    auto nonexistent = tempDir_ / "subdir";
    FileRunHistory history(nonexistent);
    history.save(makeRecord("run-1000000", 1000000));

    EXPECT_TRUE(std::filesystem::exists(nonexistent));
    auto results = history.list();
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, "run-1000000");
}

TEST_F(FileRunHistoryTest, GetNonexistentReturnsNullopt) {
    FileRunHistory history(tempDir_);
    auto result = history.get("run-nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(FileRunHistoryTest, ListEmptyReturnsEmpty) {
    FileRunHistory history(tempDir_);
    auto results = history.list();
    EXPECT_TRUE(results.empty());
}

TEST_F(FileRunHistoryTest, ConcurrentSaveAndList) {
    FileRunHistory history(tempDir_);

    std::thread writer([&history]() {
        for (int i = 0; i < 50; ++i) {
            auto record = makeRecord("run-" + std::to_string(1000000 + i), 1000000 + i);
            history.save(record);
        }
    });

    std::thread reader([&history]() {
        for (int i = 0; i < 50; ++i) {
            history.list();
        }
    });

    writer.join();
    reader.join();

    auto results = history.list();
    EXPECT_EQ(results.size(), 50u);
}
