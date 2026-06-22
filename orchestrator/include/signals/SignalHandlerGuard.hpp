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
 * On first construction, creates a non-blocking pipe and installs a signal
 * handler that writes to the pipe on SIGINT. Subsequent constructions share
 * the same pipe and handler via reference counting. Only the last destructor
 * restores the previous signal handler and closes the pipe, making multiple
 * coexisting instances safe.
 */
class SignalHandlerGuard {
public:
    /**
     * @brief Install the SIGINT handler and create the self-pipe.
     */
    SignalHandlerGuard();

    /**
     * @brief Restore the previous SIGINT handler and close the pipe on last instance.
     */
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
    bool previous_valid_{false};
    int pipe_read_{-1};
    int pipe_write_{-1};
};

}  // namespace chaos::orchestrator::signals
