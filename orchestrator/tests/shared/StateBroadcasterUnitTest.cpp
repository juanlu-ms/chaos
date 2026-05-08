/// @file StateBroadcasterUnitTest.cpp
/// @brief Unit tests for StateBroadcaster subscribe/unsubscribe and exception safety.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "shared/StateBroadcaster.hpp"
#include "shared/TargetState.hpp"

using namespace chaos::orchestrator::shared;

TargetState makeState(const std::string& id) {
    TargetState s;
    s.container_id = id;
    s.status = ContainerStatus::Running;
    return s;
}

TEST(StateBroadcasterTest, SubscribeReturnsHandle) {
    StateBroadcaster bc;
    auto h = bc.subscribe([](const TargetState&) {});
    EXPECT_NE(h, StateBroadcaster::Handle{});
}

TEST(StateBroadcasterTest, UnsubscribeRemovesCallback) {
    StateBroadcaster bc;
    std::atomic<int> count{0};
    auto h = bc.subscribe([&](const TargetState&) { count++; });
    bc.unsubscribe(h);
    bc.broadcast(makeState("x"));
    EXPECT_EQ(count.load(), 0);
}

TEST(StateBroadcasterTest, BroadcastReachesAllSubscribers) {
    StateBroadcaster bc;
    std::atomic<int> sum{0};
    auto h1 = bc.subscribe([&](const TargetState&) { sum++; });
    auto h2 = bc.subscribe([&](const TargetState&) { sum++; });
    bc.broadcast(makeState("x"));
    EXPECT_EQ(sum.load(), 2);
    bc.unsubscribe(h1);
    bc.unsubscribe(h2);
}

TEST(StateBroadcasterTest, ConcurrentSubscribe) {
    StateBroadcaster bc;
    std::atomic<int> count{0};
    std::vector<StateBroadcaster::Handle> handles(10);
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.push_back(std::thread([&, i]() {
            auto h = bc.subscribe([&](const TargetState&) { count++; });
            handles[i] = h;
        }));
    }
    for (auto& t : threads) t.join();
    bc.broadcast(makeState("x"));
    EXPECT_EQ(count.load(), 10);
    for (auto h : handles) bc.unsubscribe(h);
}

TEST(StateBroadcasterTest, ConcurrentUnsubscribe) {
    StateBroadcaster bc;
    std::atomic<int> count{0};
    std::vector<StateBroadcaster::Handle> handles;
    for (int i = 0; i < 10; i++) {
        handles.push_back(bc.subscribe([&](const TargetState&) { count++; }));
    }
    std::vector<std::thread> threads;
    for (auto h : handles) {
        threads.push_back(std::thread([&, h]() { bc.unsubscribe(h); }));
    }
    for (auto& t : threads) t.join();
    bc.broadcast(makeState("x"));
    EXPECT_EQ(count.load(), 0);
}

TEST(StateBroadcasterTest, ExceptionInCallbackDoesNotCrashOthers) {
    StateBroadcaster bc;
    std::atomic<int> count{0};
    auto h1 = bc.subscribe([&](const TargetState&) { throw std::runtime_error("bad"); });
    auto h2 = bc.subscribe([&](const TargetState&) { count++; });
    bc.broadcast(makeState("x"));
    EXPECT_EQ(count.load(), 1);
    bc.unsubscribe(h1);
    bc.unsubscribe(h2);
}
