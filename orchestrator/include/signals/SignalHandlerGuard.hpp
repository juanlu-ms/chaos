/**
 * @file SignalHandlerGuard.hpp
 * @brief RAII guard for SIGINT handling via self-pipe.
 */

#pragma once

#include <csignal>
#include <stop_token>

namespace chaos::orchestrator::signals {

/**
 * @brief RAII guard that installs a SIGINT handler using the self-pipe trick.
 *
 * On construction, creates a non-blocking pipe and installs a signal handler
 * that writes to the pipe on SIGINT. The read end can be polled to detect
 * signal delivery. On destruction, restores the previous signal handler and
 * closes the pipe.
 */
class SignalHandlerGuard {
public:
    SignalHandlerGuard();
    ~SignalHandlerGuard() noexcept;

    SignalHandlerGuard(const SignalHandlerGuard&) = delete;
    SignalHandlerGuard& operator=(const SignalHandlerGuard&) = delete;
    SignalHandlerGuard(SignalHandlerGuard&&) = delete;
    SignalHandlerGuard& operator=(SignalHandlerGuard&&) = delete;

    /**
     * @brief Get the file descriptor to poll for signal delivery.
     * @return Non-blocking file descriptor, or -1 if pipe creation failed.
     */
    [[nodiscard]] int readEnd() const;

    /**
     * @brief Get a stop token that is signaled when SIGINT is received.
     * @return Stop token from the internal stop_source.
     */
    [[nodiscard]] std::stop_token token() const;

    /**
     * @brief Check if the guard is in a valid state (pipe created successfully).
     * @return True if the pipe and signal handler were set up correctly.
     */
    [[nodiscard]] bool valid() const;

private:
    struct sigaction previous_ {};
    bool previous_valid_{false};
    int pipe_read_{-1};
    int pipe_write_{-1};
};

}  // namespace chaos::orchestrator::signals
