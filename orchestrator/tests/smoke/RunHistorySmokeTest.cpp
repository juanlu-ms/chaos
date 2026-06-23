#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "history/FileRunHistory.hpp"
#include "history/RunRecord.hpp"

using namespace chaos::orchestrator::history;

class RunHistorySmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir_ = std::filesystem::temp_directory_path() / ("chaos_smoke_history_" + std::to_string(std::rand()));
        std::filesystem::create_directories(tempDir_);
        history_ = std::make_unique<FileRunHistory>(tempDir_, 10);
    }
    void TearDown() override {
        history_.reset();
        std::filesystem::remove_all(tempDir_);
    }

    std::filesystem::path tempDir_;
    std::unique_ptr<FileRunHistory> history_;

    static RunRecord makeRecord(const std::string& id, int64_t started_at) {
        RunRecord r;
        r.summary.id = id;
        r.summary.started_at_unix = started_at;
        r.summary.status = "completed";
        r.summary.run_result.manifest_name = "smoke-test";
        r.summary.run_result.target_id = "target-1";
        r.summary.run_result.passed = true;
        return r;
    }
};

TEST_F(RunHistorySmokeTest, SaveAndList) {
    history_->save(makeRecord("run-1000", 1000));
    auto list = history_->list();
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].id, "run-1000");
}

TEST_F(RunHistorySmokeTest, SaveAndGet) {
    history_->save(makeRecord("run-2000", 2000));
    auto record = history_->get("run-2000");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->summary.id, "run-2000");
    EXPECT_EQ(record->summary.run_result.manifest_name, "smoke-test");
}

TEST_F(RunHistorySmokeTest, GetNonexistent) {
    auto record = history_->get("run-999");
    EXPECT_FALSE(record.has_value());
}

TEST_F(RunHistorySmokeTest, Remove) {
    history_->save(makeRecord("run-3000", 3000));
    EXPECT_TRUE(history_->remove("run-3000"));
    EXPECT_FALSE(history_->get("run-3000").has_value());
}

TEST_F(RunHistorySmokeTest, Clear) {
    history_->save(makeRecord("run-4000", 4000));
    history_->save(makeRecord("run-5000", 5000));
    history_->clear();
    EXPECT_TRUE(history_->list().empty());
}

TEST_F(RunHistorySmokeTest, RetentionPruning) {
    auto smallHistory = std::make_unique<FileRunHistory>(tempDir_ / "prune", 3);
    smallHistory->save(makeRecord("run-100", 100));
    smallHistory->save(makeRecord("run-200", 200));
    smallHistory->save(makeRecord("run-300", 300));
    smallHistory->save(makeRecord("run-400", 400));  // triggers prune
    auto list = smallHistory->list();
    ASSERT_EQ(list.size(), 3u);
    EXPECT_FALSE(smallHistory->get("run-100").has_value());
    EXPECT_TRUE(smallHistory->get("run-200").has_value());
    EXPECT_TRUE(smallHistory->get("run-300").has_value());
    EXPECT_TRUE(smallHistory->get("run-400").has_value());
}

TEST_F(RunHistorySmokeTest, DirAutoCreate) {
    auto newDir = tempDir_ / "nonexistent" / "subdir";
    auto fh = std::make_unique<FileRunHistory>(newDir, 10);
    fh->save(makeRecord("run-6000", 6000));
    EXPECT_TRUE(std::filesystem::exists(newDir));
    fh.reset();
    std::filesystem::remove_all(tempDir_ / "nonexistent");
}
