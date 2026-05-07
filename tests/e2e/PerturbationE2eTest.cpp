#include <gtest/gtest.h>

#include <string>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "shared/ContainerStatus.hpp"

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

    std::shared_ptr<IContainerEngine> engine_;
    std::string containerId_;
};

TEST_F(PerturbationE2eTest, KillPerturbationStopsAndRestartsContainer) {
    Target target{containerId_};
    Perturbation spec{"kill", {}};
    auto perturbation = PerturbationFactory().create(engine_, target, spec);

    perturbation->apply();
    const auto statusAfterKill = engine_->getStatus(containerId_);
    EXPECT_TRUE(statusAfterKill == ContainerStatus::Exited || statusAfterKill == ContainerStatus::Dead);

    perturbation->revert();
    const auto statusAfterRevert = engine_->getStatus(containerId_);
    EXPECT_EQ(statusAfterRevert, ContainerStatus::Running);
}

TEST_F(PerturbationE2eTest, MemoryCapPerturbationApplyAndRevert) {
    Target target{containerId_};
    Perturbation spec{"memory_cap", Parameters{{"limit_bytes", "33554432"}}};  // 32 MiB
    auto perturbation = PerturbationFactory().create(engine_, target, spec);

    const auto sysInfo = engine_->getSystemInfo();

    perturbation->apply();
    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);

    perturbation->revert();
    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);

    const auto limitOutput = engine_->exec(containerId_,
                                           "sh -c 'cat /sys/fs/cgroup/memory.max 2>/dev/null || "
                                           "cat /sys/fs/cgroup/memory/memory.limit_in_bytes 2>/dev/null'");
    try {
        int64_t limitBytes = std::stoll(limitOutput);
        EXPECT_GE(limitBytes, sysInfo.memTotal)
            << "Memory limit " << limitBytes << " not restored to host " << sysInfo.memTotal;
    } catch (const std::exception&) {
        auto result = engine_->exec(containerId_, "echo ok");
        EXPECT_NE(result.find("ok"), std::string::npos) << "Container not functional after memory cap revert";
    }
}

TEST_F(PerturbationE2eTest, CpuCapPerturbationApplyAndRevert) {
    Target target{containerId_};
    Perturbation spec{"cpu_cap", Parameters{{"cpu_cores", "0.5"}}};
    auto perturbation = PerturbationFactory().create(engine_, target, spec);

    perturbation->apply();
    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);

    perturbation->revert();
    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);

    const auto cpuOutput = engine_->exec(containerId_,
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
    auto result = engine_->exec(containerId_, "echo ok");
    EXPECT_NE(result.find("ok"), std::string::npos) << "Container not functional after CPU cap revert";
}

TEST_F(PerturbationE2eTest, KillPerturbationIsReentrant) {
    Target target{containerId_};
    Perturbation spec{"kill", {}};
    auto perturbation = PerturbationFactory().create(engine_, target, spec);

    perturbation->apply();
    const auto status1 = engine_->getStatus(containerId_);
    EXPECT_TRUE(status1 == ContainerStatus::Exited || status1 == ContainerStatus::Dead);

    perturbation->revert();
    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);

    perturbation->apply();
    const auto status2 = engine_->getStatus(containerId_);
    EXPECT_TRUE(status2 == ContainerStatus::Exited || status2 == ContainerStatus::Dead);

    perturbation->revert();
    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);
}
