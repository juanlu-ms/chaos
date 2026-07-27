/**
 * @file PerturbationEngine.cpp
 * @brief Implementation for the engine that manages perturbations.
 */

#include "perturbations/PerturbationEngine.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cstddef>
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

    {
        std::lock_guard failures_lock(apply_failures_mutex_);
        apply_failures_.clear();
    }

    active_threads_.reserve(active_threads_.size() + perturbations.size());

    for (std::size_t index = 0; index < perturbations.size(); ++index) {
        auto internal_token = stop_source_.get_token();
        active_threads_.emplace_back([this, index, duration, perturbation = std::move(perturbations[index]),
                                      internal_token, external_stop]() mutable {
            const auto label = fmt::format("{} (#{})", perturbation->type(), index);
            SPDLOG_DEBUG("Perturbation task starting: {}", label);

            try {
                perturbation->apply();
            } catch (const std::exception& e) {
                recordApplyFailure(label, e.what());
                return;
            } catch (...) {
                recordApplyFailure(label, "unknown exception");
                return;
            }

            {
                std::unique_lock lock(threads_mutex_);
                cancel_cv_.wait_for(lock, external_stop, duration,
                                    [&internal_token] { return internal_token.stop_requested(); });
            }

            // A failed revert leaves the target dirty, but the fault did reach it, so the run
            // verdict stands and this is reported as an operational problem only.
            try {
                perturbation->revert();
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Perturbation {} failed to revert: {}. Target may be left in a perturbed state", label,
                             e.what());
            } catch (...) {
                SPDLOG_ERROR(
                    "Perturbation {} failed to revert: unknown exception. Target may be left in a perturbed state",
                    label);
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

void PerturbationEngine::recordApplyFailure(std::string_view label, std::string_view reason) {
    SPDLOG_ERROR("Perturbation {} failed to apply: {}", label, reason);
    std::lock_guard lock(apply_failures_mutex_);
    apply_failures_.push_back(fmt::format("{}: {}", label, reason));
}

std::vector<std::string> PerturbationEngine::applyFailures() const {
    std::lock_guard lock(apply_failures_mutex_);
    return apply_failures_;
}

}  // namespace chaos::orchestrator::perturbations
