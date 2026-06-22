#pragma once

#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace chaos::orchestrator::interfaces::cli {

/**
 * @brief Singleton coordinator for progress-line rendering with thread-safe line clearing.
 */
class ProgressCoordinator {
public:
    /**
     * @brief Get the singleton instance.
     * @return Reference to the singleton ProgressCoordinator.
     */
    static ProgressCoordinator& instance() {
        static ProgressCoordinator inst;
        return inst;
    }

    /**
     * @brief Enable or disable progress rendering.
     * @param active Whether progress rendering is active.
     */
    void setActive(bool active) { active_.store(active, std::memory_order_relaxed); }

    /**
     * @brief Render a line to stderr when progress is active.
     * @param line The line to render.
     */
    void renderLine(const std::string& line) {
        if (!active_.load(std::memory_order_relaxed)) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_.load(std::memory_order_relaxed)) return;
        std::fputs(line.c_str(), stderr);
        std::fflush(stderr);
    }

    /**
     * @brief Clear the current progress line, execute a function, and flush.
     * @tparam Fn Callable type.
     * @param fn The function to execute while holding the progress lock.
     */
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

    /**
     * @brief Clear the progress line and deactivate progress rendering.
     */
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

/**
 * @brief spdlog sink wrapper that clears progress lines before writing log messages.
 */
class ProgressAwareSink : public spdlog::sinks::sink {
public:
    /**
     * @brief Construct a progress-aware sink around an inner sink.
     * @param inner The inner spdlog sink to wrap.
     */
    explicit ProgressAwareSink(std::shared_ptr<spdlog::sinks::sink> inner) : inner_(std::move(inner)) {}

    /**
     * @brief Log a message, clearing any progress line first.
     * @param msg The log message to write.
     */
    void log(const spdlog::details::log_msg& msg) override {
        ProgressCoordinator::instance().clearLineAndRun([this, &msg] { inner_->log(msg); });
    }

    /**
     * @brief Flush the inner sink.
     */
    void flush() override { inner_->flush(); }

    /**
     * @brief Set the log pattern on the inner sink.
     * @param pattern The spdlog pattern string.
     */
    void set_pattern(const std::string& pattern) override { inner_->set_pattern(pattern); }

    /**
     * @brief Set the formatter on the inner sink.
     * @param sink_formatter The spdlog formatter to install.
     */
    void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override {
        inner_->set_formatter(std::move(sink_formatter));
    }

private:
    std::shared_ptr<spdlog::sinks::sink> inner_;
};

/**
 * @brief Wrap all sinks of the default spdlog logger with ProgressAwareSink instances.
 */
inline void installProgressAwareSink() {
    auto* logger = spdlog::default_logger_raw();
    if (logger == nullptr) return;
    auto& sinks = logger->sinks();
    for (auto& sink : sinks) {
        sink = std::make_shared<ProgressAwareSink>(sink);
    }
}

}  // namespace chaos::orchestrator::interfaces::cli
