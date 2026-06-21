#pragma once

#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace chaos::orchestrator::interfaces::cli {

class ProgressCoordinator {
public:
    static ProgressCoordinator& instance() {
        static ProgressCoordinator inst;
        return inst;
    }

    void setActive(bool active) { active_.store(active, std::memory_order_relaxed); }

    void renderLine(const std::string& line) {
        if (!active_.load(std::memory_order_relaxed)) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_.load(std::memory_order_relaxed)) return;
        std::fputs(line.c_str(), stderr);
        std::fflush(stderr);
    }

    template <typename Fn>
    void clearLineAndRun(Fn fn) {
        if (!active_.load(std::memory_order_relaxed)) {
            fn();
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_.load(std::memory_order_relaxed)) {
            static constexpr const char kClearLine[] =
                "\r                                                                                \r";
            std::fputs(kClearLine, stderr);
        }
        fn();
        std::fflush(stderr);
    }

    void finish() {
        if (!active_.load(std::memory_order_relaxed)) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_.load(std::memory_order_relaxed)) {
            static constexpr const char kClearLine[] =
                "\r                                                                                \r";
            std::fputs(kClearLine, stderr);
            std::fflush(stderr);
            active_.store(false, std::memory_order_relaxed);
        }
    }

private:
    ProgressCoordinator() = default;
    std::mutex mutex_;
    std::atomic<bool> active_{false};
};

class ProgressAwareSink : public spdlog::sinks::sink {
public:
    explicit ProgressAwareSink(std::shared_ptr<spdlog::sinks::sink> inner) : inner_(std::move(inner)) {}

    void log(const spdlog::details::log_msg& msg) override {
        ProgressCoordinator::instance().clearLineAndRun([this, &msg] { inner_->log(msg); });
    }

    void flush() override { inner_->flush(); }

    void set_pattern(const std::string& pattern) override { inner_->set_pattern(pattern); }

    void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override {
        inner_->set_formatter(std::move(sink_formatter));
    }

private:
    std::shared_ptr<spdlog::sinks::sink> inner_;
};

inline void installProgressAwareSink() {
    auto* logger = spdlog::default_logger_raw();
    if (logger == nullptr) return;
    auto& sinks = logger->sinks();
    for (auto& sink : sinks) {
        sink = std::make_shared<ProgressAwareSink>(sink);
    }
}

}  // namespace chaos::orchestrator::interfaces::cli
