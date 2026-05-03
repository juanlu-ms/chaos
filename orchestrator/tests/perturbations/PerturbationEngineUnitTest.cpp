/// @file PerturbationEngineUnitTest.cpp
/// @brief Unit tests for PerturbationEngine lifecycle behavior.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "perturbations/IPerturbation.hpp"
#include "perturbations/PerturbationEngine.hpp"

using namespace chaos::orchestrator;

namespace {

class RecordingPerturbation final : public perturbations::IPerturbation {
public:
    RecordingPerturbation(std::shared_ptr<std::atomic<int>> apply_count,
                          std::shared_ptr<std::atomic<int>> revert_count,
                          std::shared_ptr<std::promise<void>> applied,
                          std::shared_ptr<std::promise<void>> reverted)
        : apply_count_(std::move(apply_count)),
          revert_count_(std::move(revert_count)),
          applied_(std::move(applied)),
          reverted_(std::move(reverted)) {}

    void apply() override {
        apply_count_->fetch_add(1);
        if (applied_) {
            applied_->set_value();
        }
    }

    void revert() override {
        revert_count_->fetch_add(1);
        if (reverted_) {
            reverted_->set_value();
        }
    }

private:
    std::shared_ptr<std::atomic<int>> apply_count_;
    std::shared_ptr<std::atomic<int>> revert_count_;
    std::shared_ptr<std::promise<void>> applied_;
    std::shared_ptr<std::promise<void>> reverted_;
};

class ThrowingPerturbation final : public perturbations::IPerturbation {
public:
    void apply() override { throw std::runtime_error("apply failure"); }
    void revert() override { throw std::runtime_error("revert failure"); }
};

}  // namespace

/**
 * @test Verifies scheduleAllAsync applies and reverts perturbations.
 */
TEST(PerturbationEngineUnitTest, ScheduleAllAsyncAppliesAndReverts) {
    perturbations::PerturbationEngine engine;

    auto apply_count = std::make_shared<std::atomic<int>>(0);
    auto revert_count = std::make_shared<std::atomic<int>>(0);
    auto applied = std::make_shared<std::promise<void>>();
    auto reverted = std::make_shared<std::promise<void>>();

    std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations;
    perturbations.push_back(std::make_unique<RecordingPerturbation>(apply_count, revert_count, applied, reverted));

    engine.scheduleAllAsync(std::move(perturbations), std::chrono::seconds(0));

    auto applied_future = applied->get_future();
    ASSERT_EQ(applied_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    engine.waitForTeardown();

    auto reverted_future = reverted->get_future();
    ASSERT_EQ(reverted_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    EXPECT_EQ(apply_count->load(), 1);
    EXPECT_EQ(revert_count->load(), 1);
}

/**
 * @test Verifies cancel is idempotent and triggers teardown.
 */
TEST(PerturbationEngineUnitTest, CancelIsIdempotent) {
    perturbations::PerturbationEngine engine;

    auto apply_count = std::make_shared<std::atomic<int>>(0);
    auto revert_count = std::make_shared<std::atomic<int>>(0);
    auto applied = std::make_shared<std::promise<void>>();
    auto reverted = std::make_shared<std::promise<void>>();

    std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations;
    perturbations.push_back(std::make_unique<RecordingPerturbation>(apply_count, revert_count, applied, reverted));

    engine.scheduleAllAsync(std::move(perturbations), std::chrono::seconds(60));

    auto applied_future = applied->get_future();
    ASSERT_EQ(applied_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    std::thread cancel_thread([&engine]() { engine.cancel(); });
    engine.cancel();
    cancel_thread.join();

    engine.waitForTeardown();

    auto reverted_future = reverted->get_future();
    ASSERT_EQ(reverted_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    EXPECT_EQ(apply_count->load(), 1);
    EXPECT_EQ(revert_count->load(), 1);
}

/**
 * @test Verifies waitForTeardown handles task exceptions.
 */
TEST(PerturbationEngineUnitTest, WaitForTeardownHandlesTaskExceptions) {
    perturbations::PerturbationEngine engine;

    std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations;
    perturbations.push_back(std::make_unique<ThrowingPerturbation>());

    engine.scheduleAllAsync(std::move(perturbations), std::chrono::seconds(0));

    EXPECT_NO_THROW(engine.waitForTeardown());
}

/**
 * @test Verifies destructor calls waitForTeardown.
 */
TEST(PerturbationEngineUnitTest, DestructorCallsWaitForTeardown) {
    auto apply_count = std::make_shared<std::atomic<int>>(0);
    auto revert_count = std::make_shared<std::atomic<int>>(0);
    auto applied = std::make_shared<std::promise<void>>();
    auto reverted = std::make_shared<std::promise<void>>();

    {
        perturbations::PerturbationEngine engine;
        std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbations;
        perturbations.push_back(std::make_unique<RecordingPerturbation>(apply_count, revert_count, applied, reverted));
        engine.scheduleAllAsync(std::move(perturbations), std::chrono::seconds(0));

        auto applied_future = applied->get_future();
        ASSERT_EQ(applied_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    }

    auto reverted_future = reverted->get_future();
    ASSERT_EQ(reverted_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    EXPECT_EQ(apply_count->load(), 1);
    EXPECT_EQ(revert_count->load(), 1);
}