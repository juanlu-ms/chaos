/**
 * @file PerturbationEngine.hpp
 * @brief Engine for encapsulating perturbation lifecycle management.
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "IPerturbation.hpp"

namespace chaos::orchestrator::perturbations {

/**
 * @brief Manages the safe lifecycle of perturbations.
 * Creates, applies, and guarantees reversion of perturbations.
 */
class PerturbationEngine {
public:
    /**
     * @brief Construct an empty PerturbationEngine with no active perturbations.
     */
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
    void scheduleAllAsync(std::vector<std::unique_ptr<IPerturbation>> perturbations, std::chrono::seconds duration,
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

    /**
     * @brief Get the messages of perturbations that failed to apply.
     *
     * A non-empty result means at least one fault was never injected, so the
     * run must not be reported as passing. Reset on each scheduleAllAsync call.
     *
     * @return List of failure messages, one per perturbation whose apply() threw,
     *         each prefixed with the perturbation type and its scheduling index.
     */
    [[nodiscard]] std::vector<std::string> applyFailures() const;

private:
    // Log a failed apply() once and record it for applyFailures(). Called from task threads.
    void recordApplyFailure(std::string_view label, std::string_view reason);

    // Lock order, when both are needed: threads_mutex_ before apply_failures_mutex_.
    // Tasks record a failure and release apply_failures_mutex_ before taking
    // threads_mutex_ to wait, so the reverse order must never be introduced.
    std::vector<std::jthread> active_threads_;
    std::mutex threads_mutex_;
    // condition_variable_any so tasks can wait on the caller's stop_token without a
    // hand-rolled notify, which would be prone to lost wakeups.
    std::condition_variable_any cancel_cv_;
    std::stop_source stop_source_;

    // mutable because the mutex guards apply_failures_ rather than being part of the
    // engine's observable state, and applyFailures() is const.
    mutable std::mutex apply_failures_mutex_;
    std::vector<std::string> apply_failures_;
};

}  // namespace chaos::orchestrator::perturbations
