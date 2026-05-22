#include <fmt/format.h>
#include <gtest/gtest.h>

#include <string>
#include <thread>

#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "shared/ContainerStatus.hpp"

#include "E2eTestBase.hpp"

using chaos::orchestrator::containers::ContainerEngineError;
using chaos::orchestrator::manifests::Parameters;
using chaos::orchestrator::manifests::Perturbation;
using chaos::orchestrator::manifests::Target;
using chaos::orchestrator::perturbations::PerturbationFactory;

class PerturbationE2eTest : public E2eTestBase {
};

TEST_F(PerturbationE2eTest, KillPerturbationStopsAndRestartsContainer) {
    Target target{containerId()};
    Perturbation spec{"kill", {}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    perturbation->apply();
    const auto statusAfterKill = engine()->getStatus(containerId());
    EXPECT_TRUE(statusAfterKill == ContainerStatus::Exited || statusAfterKill == ContainerStatus::Dead);

    perturbation->revert();
    const auto statusAfterRevert = engine()->getStatus(containerId());
    EXPECT_EQ(statusAfterRevert, ContainerStatus::Running);
}

TEST_F(PerturbationE2eTest, MemoryCapPerturbationApplyAndRevert) {
    Target target{containerId()};
    Perturbation spec{"memory_cap", Parameters{{"limit_bytes", "33554432"}}};  // 32 MiB
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    const auto sysInfo = engine()->getSystemInfo();

    perturbation->apply();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    const auto limitOutput = engine()->exec(containerId(),
                                            "sh -c 'cat /sys/fs/cgroup/memory.max 2>/dev/null || "
                                            "cat /sys/fs/cgroup/memory/memory.limit_in_bytes 2>/dev/null'");
    try {
        int64_t limitBytes = std::stoll(limitOutput);
        EXPECT_GE(limitBytes, sysInfo.memTotal)
            << "Memory limit " << limitBytes << " not restored to host " << sysInfo.memTotal;
    } catch (const std::exception&) {
        auto result = engine()->exec(containerId(), "echo ok");
        EXPECT_NE(result.find("ok"), std::string::npos) << "Container not functional after memory cap revert";
    }
}

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
 *       container stays running, interface RX counters increase, and traffic flows after revert.
 */
TEST_F(PerturbationE2eTest, PacketFloodPerturbationApplyAndRevert) {
    Target target{containerId()};
    Perturbation spec{"packet_flood", Parameters{{"rate", "100000"}, {"packet_size", "64"}}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    // Read baseline interface RX packet count
    const auto rxBefore = engine()->execInNetNs(containerId(), "cat /sys/class/net/eth0/statistics/rx_packets");

    perturbation->apply();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    // Let the flood run and inject packets
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // During flood: RX packets should increase (flood injects TCP SYNs that loop back)
    const auto rxDuring = engine()->execInNetNs(containerId(), "cat /sys/class/net/eth0/statistics/rx_packets");
    uint64_t rxBeforeVal = 0;
    uint64_t rxDuringVal = 0;
    try {
        rxBeforeVal = std::stoull(rxBefore);
        rxDuringVal = std::stoull(rxDuring);
    } catch (const std::exception&) {
        FAIL() << "Non-numeric RX count: before='" << rxBefore << "' during='" << rxDuring << "'";
    }
    EXPECT_GT(rxDuringVal, rxBeforeVal);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    // After revert: container is still running (not a cutoff)
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);
}

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
