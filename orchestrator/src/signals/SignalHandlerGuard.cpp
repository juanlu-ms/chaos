/**
 * @file SignalHandlerGuard.cpp
 * @brief Implementation of RAII SIGINT signal handler guard.
 */

#include "signals/SignalHandlerGuard.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <csignal>

namespace chaos::orchestrator::signals {

namespace {

std::stop_source g_stop_source;
int g_signal_pipe[2] = {-1, -1};

void signalHandler(int) {
    g_stop_source.request_stop();
    int dummy = 1;
    [[maybe_unused]] ssize_t result = ::write(g_signal_pipe[1], &dummy, sizeof(dummy));
}

}  // namespace

SignalHandlerGuard::SignalHandlerGuard() {
    if (::pipe(g_signal_pipe) != 0) {
        g_signal_pipe[0] = g_signal_pipe[1] = -1;
        return;
    }
    if (::fcntl(g_signal_pipe[0], F_SETFL, O_NONBLOCK) == -1) {
        ::close(g_signal_pipe[0]);
        ::close(g_signal_pipe[1]);
        g_signal_pipe[0] = g_signal_pipe[1] = -1;
        return;
    }
    struct sigaction sa = {};
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    previous_valid_ = (::sigaction(SIGINT, &sa, &previous_) == 0);

    pipe_read_ = g_signal_pipe[0];
    pipe_write_ = g_signal_pipe[1];
}

SignalHandlerGuard::~SignalHandlerGuard() noexcept {
    if (previous_valid_) {
        ::sigaction(SIGINT, &previous_, nullptr);
    }
    if (pipe_read_ != -1) {
        ::close(pipe_read_);
    }
    if (pipe_write_ != -1) {
        ::close(pipe_write_);
    }
    g_signal_pipe[0] = -1;
    g_signal_pipe[1] = -1;
}

int SignalHandlerGuard::readEnd() const { return pipe_read_; }

std::stop_token SignalHandlerGuard::token() const { return g_stop_source.get_token(); }

bool SignalHandlerGuard::valid() const { return pipe_read_ != -1 && previous_valid_; }

}  // namespace chaos::orchestrator::signals
