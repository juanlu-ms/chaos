/**
 * @file PerturbationEngine.cpp
 * @brief Implementation for the engine that manages perturbations.
 */

#include "perturbations/PerturbationEngine.hpp"

#include <spdlog/spdlog.h>

#include <future>
#include <utility>

namespace chaos::orchestrator::perturbations {

PerturbationEngine::~PerturbationEngine() {
    cancel();
    waitForTeardown();
}

void PerturbationEngine::scheduleAllAsync(std::vector<std::unique_ptr<IPerturbation>> perturbations,
                                          std::chrono::seconds duration) {
    {
        std::lock_guard<std::mutex> lock(cancel_mutex_);
        cancel_requested_.store(false);
    }

    std::lock_guard<std::mutex> tasks_lock(tasks_mutex_);
    active_tasks_.reserve(active_tasks_.size() + perturbations.size());

    for (auto& perturbation : perturbations) {
        active_tasks_.emplace_back(
            std::async(std::launch::async, [this, duration, perturbation = std::move(perturbation)]() mutable {
                try {
                    perturbation->apply();
                    std::unique_lock<std::mutex> lock(cancel_mutex_);
                    cancel_cv_.wait_for(lock, duration, [this]() { return cancel_requested_.load(); });
                    perturbation->revert();
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("Perturbation task failed: {}", e.what());
                } catch (...) {
                    SPDLOG_ERROR("Perturbation task failed: unknown error");
                }
            }));
    }
}

void PerturbationEngine::cancel() {
    bool expected = false;
    if (!cancel_requested_.compare_exchange_strong(expected, true)) {
        return;
    }

    cancel_cv_.notify_all();
}

void PerturbationEngine::waitForTeardown() {
    std::vector<std::future<void>> tasks;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks.swap(active_tasks_);
    }

    bool had_errors = false;
    for (auto& task : tasks) {
        if (!task.valid()) {
            continue;
        }
        try {
            task.get();
        } catch (const std::exception& e) {
            had_errors = true;
            SPDLOG_ERROR("Perturbation task teardown failed: {}", e.what());
        } catch (...) {
            had_errors = true;
            SPDLOG_ERROR("Perturbation task teardown failed: unknown error");
        }
    }

    if (!had_errors) {
        SPDLOG_INFO("Perturbation tasks torn down successfully.");
    }
}

}  // namespace chaos::orchestrator::perturbations
