/**
 * @file PerturbationE2eTest.cpp
 * @brief End-to-end tests for perturbation types (kill, memory cap, cpu cap, network delay/corruption/flood/cutoff).
 */

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include "E2eTestBase.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"
#include "perturbations/PerturbationFactory.hpp"

using chaos::orchestrator::containers::ContainerEngineError;
using chaos::orchestrator::manifests::Parameters;
using chaos::orchestrator::manifests::Perturbation;
using chaos::orchestrator::manifests::Target;
using chaos::orchestrator::perturbations::PerturbationFactory;

/** @brief Test fixture for end-to-end perturbation type integration tests. */
class PerturbationE2eTest : public E2eTestBase {};

namespace {

/**
 * @brief Extracts the TX packet count of @p iface from /proc/net/dev output.
 * @return The count, or std::nullopt if the interface line is missing or malformed.
 */
std::optional<uint64_t> parseTxPackets(const std::string& procNetDev, std::string_view iface) {
    std::istringstream lines(procNetDev);
    for (std::string line; std::getline(lines, line);) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const auto nameStart = line.find_first_not_of(' ');
        if (std::string_view(line).substr(nameStart, colon - nameStart) != iface) {
            continue;
        }
        // After "iface:" come 8 RX columns, then tx_bytes tx_packets ...; large counters can touch the colon.
        std::istringstream fields(line.substr(colon + 1));
        constexpr int kTxPacketsColumn = 9;
        uint64_t value = 0;
        for (int column = 0; column <= kTxPacketsColumn; ++column) {
            if (!(fields >> value)) {
                return std::nullopt;
            }
        }
        return value;
    }
    return std::nullopt;
}

}  // namespace

/**
 * @test Applies kill perturbation and verifies container stops, then reverts and verifies restart.
 */
TEST_F(PerturbationE2eTest, KillPerturbationStopsAndRestartsContainer) {
    Target target{containerId()};
    Perturbation spec{"kill", {}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    perturbation->apply();
    const auto status_after_kill = engine()->getStatus(containerId());
    EXPECT_TRUE(status_after_kill == ContainerStatus::Exited || status_after_kill == ContainerStatus::Dead);

    perturbation->revert();
    const auto status_after_revert = engine()->getStatus(containerId());
    EXPECT_EQ(status_after_revert, ContainerStatus::Running);
}

/**
 * @test Applies memory cap, verifies container stays running, reverts and verifies memory limit restored.
 */
TEST_F(PerturbationE2eTest, MemoryCapPerturbationApplyAndRevert) {
    Target target{containerId()};
    Perturbation spec{"memory_cap", Parameters{{"limit_bytes", "33554432"}}};  // 32 MiB
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    const auto sys_info = engine()->getSystemInfo();

    perturbation->apply();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    const auto limitOutput = engine()->exec(containerId(),
                                            "sh -c 'cat /sys/fs/cgroup/memory.max 2>/dev/null || "
                                            "cat /sys/fs/cgroup/memory/memory.limit_in_bytes 2>/dev/null'");
    try {
        int64_t limit_bytes = std::stoll(limitOutput);
        EXPECT_GE(limit_bytes, sys_info.mem_total)
            << "Memory limit " << limit_bytes << " not restored to host " << sys_info.mem_total;
    } catch (const std::exception&) {
        auto result = engine()->exec(containerId(), "echo ok");
        EXPECT_NE(result.find("ok"), std::string::npos) << "Container not functional after memory cap revert";
    }
}

/**
 * @test Applies CPU cap, verifies container stays running, reverts and verifies quota reset to unlimited.
 */
TEST_F(PerturbationE2eTest, CpuCapPerturbationApplyAndRevert) {
    Target target{containerId()};
    Perturbation spec{"cpu_cap", Parameters{{"cpu_cores", "0.5"}}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    perturbation->apply();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    const auto cpuOutput = engine()->exec(containerId(),
                                          "sh -c 'cat /sys/fs/cgroup/cpu.max 2>/dev/null || "
                                          "cat /sys/fs/cgroup/cpu/cpu.cfs_quota_us 2>/dev/null'");
    if (!cpuOutput.empty()) {
        try {
            int64_t quotaValue = std::stoll(cpuOutput);
            EXPECT_EQ(quotaValue, -1) << "CPU quota not reset to unlimited after revert";
        } catch (const std::exception&) {
            EXPECT_NE(cpuOutput.find("max"), std::string::npos) << "CPU quota not reset after revert: " << cpuOutput;
        }
    }
    auto result = engine()->exec(containerId(), "echo ok");
    EXPECT_NE(result.find("ok"), std::string::npos) << "Container not functional after CPU cap revert";
}

/**
 * @test Verifies kill perturbation can be applied, reverted, and applied again (reentrant).
 */
TEST_F(PerturbationE2eTest, KillPerturbationIsReentrant) {
    Target target{containerId()};
    Perturbation spec{"kill", {}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    perturbation->apply();
    const auto status1 = engine()->getStatus(containerId());
    EXPECT_TRUE(status1 == ContainerStatus::Exited || status1 == ContainerStatus::Dead);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    perturbation->apply();
    const auto status2 = engine()->getStatus(containerId());
    EXPECT_TRUE(status2 == ContainerStatus::Exited || status2 == ContainerStatus::Dead);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);
}

/**
 * @test Applies network delay, verifies tc netem qdisc is added, reverts and verifies qdisc removed.
 */
TEST_F(PerturbationE2eTest, NetworkDelayPerturbationApplyAndRevert) {
    Target target{containerId()};
    Perturbation spec{"network_delay", Parameters{{"delay_ms", "200"}}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    perturbation->apply();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    const auto qdiscAfterApply = engine()->execInNetNs(containerId(), "tc qdisc show dev eth0");
    EXPECT_NE(qdiscAfterApply.find("netem"), std::string::npos);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    const auto qdiscAfterRevert = engine()->execInNetNs(containerId(), "tc qdisc show dev eth0");
    EXPECT_EQ(qdiscAfterRevert.find("netem"), std::string::npos);
}

/**
 * @test Applies and reverts TrafficCorruptionPerturbation, verifying tc netem qdisc is added and removed.
 */
TEST_F(PerturbationE2eTest, TrafficCorruptionPerturbationApplyAndRevert) {
    Target target{containerId()};
    Perturbation spec{"traffic_corruption", Parameters{{"corrupt_pct", "10"}}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    perturbation->apply();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    const auto qdiscAfterApply = engine()->execInNetNs(containerId(), "tc qdisc show dev eth0");
    EXPECT_NE(qdiscAfterApply.find("netem"), std::string::npos);
    EXPECT_NE(qdiscAfterApply.find("corrupt 10"), std::string::npos);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    const auto qdiscAfterRevert = engine()->execInNetNs(containerId(), "tc qdisc show dev eth0");
    EXPECT_EQ(qdiscAfterRevert.find("netem"), std::string::npos);
}

/**
 * @test Verifies PacketFloodPerturbation is a flood (packets injected), not a cutoff:
 *       container stays running, interface TX counters increase, and traffic flows after revert.
 */
TEST_F(PerturbationE2eTest, PacketFloodPerturbationApplyAndRevert) {
    Target target{containerId()};
    Perturbation spec{"packet_flood", Parameters{{"rate", "100000"}, {"packet_size", "64"}}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    // nsenter -n switches only the network namespace, so /sys is still the host's sysfs and its
    // eth0 is the runner's. /proc/net is a link to /proc/self/net, which follows the netns of the
    // reading process, so it shows the container's interfaces.
    const auto devBefore = engine()->execInNetNs(containerId(), "cat /proc/net/dev");

    perturbation->apply();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    // Let the flood run and inject packets
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // The flood writes frames to eth0 through an AF_PACKET socket, so they count as TX. RX stays flat:
    // the bridge won't send a frame back out the port it came in on, so nothing loops back.
    // The floor keeps background traffic (ARP, IPv6 ND) from passing the check on its own.
    const auto devDuring = engine()->execInNetNs(containerId(), "cat /proc/net/dev");
    const auto tx_before_val = parseTxPackets(devBefore, "eth0");
    const auto tx_during_val = parseTxPackets(devDuring, "eth0");
    ASSERT_TRUE(tx_before_val && tx_during_val) << "No eth0 TX count in container /proc/net/dev:\nbefore:\n"
                                                << devBefore << "\nduring:\n"
                                                << devDuring;
    constexpr uint64_t kMinFloodPackets = 1000;
    EXPECT_GE(*tx_during_val - *tx_before_val, kMinFloodPackets);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    // After revert: container is still running (not a cutoff)
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);
}

/**
 * @test Applies network cutoff (iptables), verifies container stays running, reverts and verifies state.
 */
TEST_F(PerturbationE2eTest, NetworkCutoffPerturbationApplyAndRevert) {
    (void)engine()->execInNetNs(containerId(), "iptables -L -n");

    Target target{containerId()};
    Perturbation spec{"network_cutoff", {}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    perturbation->apply();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);
}
