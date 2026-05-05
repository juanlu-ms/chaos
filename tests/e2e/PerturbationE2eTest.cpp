#include <gtest/gtest.h>

#include "containers/ContainerEngineFactory.hpp"
#include "containers/IContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/IPerturbation.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "shared/ContainerStatus.hpp"

using chaos::orchestrator::containers::createContainerEngine;
using chaos::orchestrator::containers::IContainerEngine;
using chaos::orchestrator::manifests::Target;
using chaos::orchestrator::manifests::Perturbation;
using chaos::orchestrator::manifests::Parameters;
using chaos::orchestrator::perturbations::PerturbationFactory;
using chaos::orchestrator::shared::ContainerStatus;

class PerturbationE2eTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = createContainerEngine();
        containerId_ = engine_->createContainer("chaos-demo-target:latest", {});
        ASSERT_FALSE(containerId_.empty());
        engine_->startContainer(containerId_);
        ASSERT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);
    }

    void TearDown() override {
        if (engine_ && !containerId_.empty()) {
            try {
                engine_->removeContainer(containerId_);
            } catch (...) {
            }
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
    Perturbation spec{"memory_cap", Parameters{{"limit_bytes", "67108864"}}};
    auto perturbation = PerturbationFactory().create(engine_, target, spec);

    EXPECT_NO_THROW(perturbation->apply());
    EXPECT_NO_THROW(perturbation->revert());
}

TEST_F(PerturbationE2eTest, CpuCapPerturbationApplyAndRevert) {
    Target target{containerId_};
    Perturbation spec{"cpu_cap", Parameters{{"cpu_cores", "0.5"}}};
    auto perturbation = PerturbationFactory().create(engine_, target, spec);

    EXPECT_NO_THROW(perturbation->apply());
    EXPECT_NO_THROW(perturbation->revert());
}

TEST_F(PerturbationE2eTest, KillPerturbationIsReentrant) {
    Target target{containerId_};
    Perturbation spec{"kill", {}};
    auto perturbation = PerturbationFactory().create(engine_, target, spec);

    perturbation->apply();
    perturbation->revert();
    perturbation->apply();
    perturbation->revert();

    EXPECT_EQ(engine_->getStatus(containerId_), ContainerStatus::Running);
}
