/**
 * @file PerturbationEngine.hpp
 * @brief Engine for encapsulating perturbation lifecycle management.
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>
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

    // Delete copy and move construction and assignment
    PerturbationEngine(const PerturbationEngine&) = delete;
    PerturbationEngine& operator=(const PerturbationEngine&) = delete;
    PerturbationEngine(PerturbationEngine&&) = delete;
    PerturbationEngine& operator=(PerturbationEngine&&) = delete;

    /**
     * @brief Schedule perturbations to run asynchronously.
     * @param perturbations Perturbations to apply and later revert.
     * @param duration Duration to wait before reverting unless canceled.
     * @param external_stop Optional stop token for external cancellation.
     */
    void scheduleAllAsync(std::vector<std::unique_ptr<IPerturbation>> perturbations,
                          std::chrono::seconds duration,
                          std::stop_token external_stop = {});

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
    std::vector<std::jthread> active_threads_;
    std::mutex threads_mutex_;
    std::condition_variable cancel_cv_;
    std::stop_source stop_source_;
};

}  // namespace chaos::orchestrator::perturbations
