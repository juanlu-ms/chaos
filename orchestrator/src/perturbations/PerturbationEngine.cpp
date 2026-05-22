/**
 * @file PerturbationEngine.cpp
 * @brief Implementation for the engine that manages perturbations.
 */

#include "perturbations/PerturbationEngine.hpp"

#include <spdlog/spdlog.h>

#include <utility>

namespace chaos::orchestrator::perturbations {

PerturbationEngine::~PerturbationEngine() {
    cancel();
    waitForTeardown();
}

void PerturbationEngine::scheduleAllAsync(std::vector<std::unique_ptr<IPerturbation>> perturbations,
                                          std::chrono::seconds duration, std::stop_token external_stop) {
    std::lock_guard lock(threads_mutex_);
    stop_source_ = std::stop_source{};

    active_threads_.reserve(active_threads_.size() + perturbations.size());

    for (auto& perturbation : perturbations) {
        auto internal_token = stop_source_.get_token();
        active_threads_.emplace_back(
            [this, duration, perturbation = std::move(perturbation), internal_token, external_stop]() mutable {
                try {
                    perturbation->apply();

                    std::unique_lock lock(threads_mutex_);
                    cancel_cv_.wait_for(lock, duration, [&internal_token, &external_stop] {
                        return internal_token.stop_requested() || external_stop.stop_requested();
                    });
                    lock.unlock();

                    perturbation->revert();
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("Perturbation task failed: {}", e.what());
                }
            });
    }
}

void PerturbationEngine::cancel() {
    {
        std::lock_guard lock(threads_mutex_);
        stop_source_.request_stop();
    }
    cancel_cv_.notify_all();
}

void PerturbationEngine::waitForTeardown() {
    std::vector<std::jthread> threads;
    {
        std::lock_guard lock(threads_mutex_);
        threads.swap(active_threads_);
    }
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    SPDLOG_INFO("Perturbation tasks torn down successfully.");
}

}  // namespace chaos::orchestrator::perturbations
