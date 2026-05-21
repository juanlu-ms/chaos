#include <gtest/gtest.h>

#include <string>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "shared/ContainerStatus.hpp"

using chaos::orchestrator::containers::ContainerEngineError;
using chaos::orchestrator::containers::createContainerEngine;
using chaos::orchestrator::containers::IContainerEngine;
using chaos::orchestrator::manifests::Parameters;
using chaos::orchestrator::manifests::Perturbation;
using chaos::orchestrator::manifests::Target;
using chaos::orchestrator::perturbations::PerturbationFactory;
using chaos::orchestrator::shared::ContainerStatus;

class PerturbationE2eTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = createContainerEngine();
        engine_->buildImage("chaos-demo-target:latest", CHAOS_EXAMPLES_DIR "/demo-target/Dockerfile");
        containerId_ = engine_->createContainer("chaos-demo-target:latest", {});
        ASSERT_FALSE(containerId_.empty());
        engine_->startContainer(containerId_);
        ASSERT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);
    }

    void TearDown() override {
        if (engine_ && !containerId_.empty()) {
            try {
                engine_->removeContainer(containerId_);
            } catch (const std::exception& ex) {
                ADD_FAILURE() << "Failed to remove container " << containerId_ << " during teardown: " << ex.what();
            }
            containerId_.clear();
        }
    }

    std::shared_ptr<IContainerEngine> engine() const { return engine_; }
    const std::string& containerId() const { return containerId_; }

private:
    std::shared_ptr<IContainerEngine> engine_;
    std::string containerId_;
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

TEST_F(PerturbationE2eTest, GarbagePacketPerturbationApplyAndRevert) {
    Target target{containerId()};
    Perturbation spec{"garbage_packet", Parameters{{"corrupt_pct", "10"}}};
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

TEST_F(PerturbationE2eTest, NetworkCutoffPerturbationApplyAndRevert) {
    try {
        (void)engine()->execInNetNs(containerId(), "iptables -L -n");
    } catch (const ContainerEngineError&) {
        GTEST_SKIP() << "iptables not available on host, skipping network_cutoff e2e test";
    }

    Target target{containerId()};
    Perturbation spec{"network_cutoff", {}};
    auto perturbation = PerturbationFactory().create(engine(), target, spec);

    perturbation->apply();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);

    perturbation->revert();
    EXPECT_EQ(engine()->getStatus(containerId()), ContainerStatus::Running);
}
