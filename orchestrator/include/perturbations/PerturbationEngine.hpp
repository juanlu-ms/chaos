/**
 * @file PerturbationEngine.hpp
 * @brief Engine for encapsulating perturbation lifecycle management.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <vector>
#include "perturbations/IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Manages the safe lifecycle of perturbations.
 * Creates, applies, and guarantees reversion of perturbations.
 */
class PerturbationEngine {
public:
    PerturbationEngine() = default;

    /**
     * @brief Cancel and wait for any scheduled perturbations.
     */
    ~PerturbationEngine();

    // Delete copy construction and assignment
    PerturbationEngine(const PerturbationEngine&) = delete;
    PerturbationEngine& operator=(const PerturbationEngine&) = delete;

    /**
     * @brief Schedule perturbations to run asynchronously.
     * @param perturbations Perturbations to apply and later revert.
     * @param duration Duration to wait before reverting unless canceled.
     */
    void scheduleAllAsync(std::vector<std::unique_ptr<IPerturbation>> perturbations,
                          std::chrono::seconds duration);

    /**
     * @brief Request cancellation of any running perturbations.
     * @note Safe to call multiple times from any thread.
     */
    void cancel();

    /**
     * @brief Wait for all active tasks to complete and clear tracking.
     */
    void waitForTeardown();

private:
    std::vector<std::future<void>> active_tasks_;
    std::mutex tasks_mutex_;
    std::mutex cancel_mutex_;
    std::condition_variable cancel_cv_;
    std::atomic<bool> cancel_requested_{false};
};

}  // namespace chaos::orchestrator::perturbations
