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
std::atomic<int> g_instance_count{0};
struct sigaction g_previous {};
bool g_previous_valid{false};

void signalHandler(int) {
    g_stop_source.request_stop();
    int dummy = 1;
    [[maybe_unused]] ssize_t result = ::write(g_signal_pipe[1], &dummy, sizeof(dummy));
}

}  // namespace

SignalHandlerGuard::SignalHandlerGuard() {
    auto count = g_instance_count.fetch_add(1, std::memory_order_acq_rel);
    if (count == 0) {
        if (::pipe(g_signal_pipe) != 0) {
            g_signal_pipe[0] = g_signal_pipe[1] = -1;
            g_instance_count.fetch_sub(1, std::memory_order_acq_rel);
            return;
        }
        if (::fcntl(g_signal_pipe[0], F_SETFL, O_NONBLOCK) == -1) {
            ::close(g_signal_pipe[0]);
            ::close(g_signal_pipe[1]);
            g_signal_pipe[0] = g_signal_pipe[1] = -1;
            g_instance_count.fetch_sub(1, std::memory_order_acq_rel);
            return;
        }
        struct sigaction sa = {};
        sa.sa_handler = signalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        g_previous_valid = (::sigaction(SIGINT, &sa, &g_previous) == 0);
    }

    pipe_read_ = g_signal_pipe[0];
    pipe_write_ = g_signal_pipe[1];
    previous_valid_ = g_previous_valid;
}

SignalHandlerGuard::~SignalHandlerGuard() noexcept {
    auto count = g_instance_count.fetch_sub(1, std::memory_order_acq_rel);
    if (count == 1) {
        if (previous_valid_) {
            ::sigaction(SIGINT, &g_previous, nullptr);
        }
        if (g_signal_pipe[0] != -1) {
            ::close(g_signal_pipe[0]);
        }
        if (g_signal_pipe[1] != -1) {
            ::close(g_signal_pipe[1]);
        }
        g_signal_pipe[0] = -1;
        g_signal_pipe[1] = -1;
    }
}

int SignalHandlerGuard::readEnd() const { return pipe_read_; }

std::stop_token SignalHandlerGuard::token() const { return g_stop_source.get_token(); }

bool SignalHandlerGuard::valid() const { return pipe_read_ != -1 && previous_valid_; }

}  // namespace chaos::orchestrator::signals
