/**
 * @file SignalHandlerGuardUnitTest.cpp
 * @brief Unit tests for SignalHandlerGuard RAII signal handler.
 */

#include <gtest/gtest.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <thread>

#include "signals/SignalHandlerGuard.hpp"

using namespace chaos::orchestrator::signals;

/**
 * @test Verifies constructor does not throw.
 */
TEST(SignalHandlerGuardTest, ConstructorDoesNotThrow) {
    EXPECT_NO_THROW({ SignalHandlerGuard guard; });
}

/**
 * @test Verifies guard is valid after construction.
 */
TEST(SignalHandlerGuardTest, IsValidAfterConstruction) {
    SignalHandlerGuard guard;
    EXPECT_TRUE(guard.valid());
}

/**
 * @test Verifies stop token is not requested initially.
 */
TEST(SignalHandlerGuardTest, StopTokenIsNotRequestedInitially) {
    SignalHandlerGuard guard;
    EXPECT_FALSE(guard.token().stop_requested());
}

/**
 * @test Verifies readEnd returns a valid file descriptor.
 */
TEST(SignalHandlerGuardTest, ReadEndReturnsValidFd) {
    SignalHandlerGuard guard;
    EXPECT_GE(guard.readEnd(), 0);
}

/**
 * @test Verifies sending SIGINT writes to the pipe (readEnd becomes readable).
 */
TEST(SignalHandlerGuardTest, SignalWritesToPipe) {
    SignalHandlerGuard guard;
    ASSERT_TRUE(guard.valid());

    std::raise(SIGINT);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    int dummy;
    ssize_t n = ::read(guard.readEnd(), &dummy, sizeof(dummy));
    EXPECT_GT(n, 0);
}

/**
 * @test Verifies stop_requested() becomes true after SIGINT.
 */
TEST(SignalHandlerGuardTest, StopTokenIsRequestedAfterSignal) {
    SignalHandlerGuard guard;
    ASSERT_TRUE(guard.valid());

    EXPECT_FALSE(guard.token().stop_requested());

    std::raise(SIGINT);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(guard.token().stop_requested());
}
