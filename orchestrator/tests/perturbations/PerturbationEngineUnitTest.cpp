/// @file PerturbationEngineUnitTest.cpp
/// @brief Unit tests for PerturbationEngine lifecycle behavior.

#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/PerturbationEngine.hpp"

using namespace testing;
using namespace chaos::orchestrator;

namespace {

manifests::ChaosManifest makeManifest(std::vector<manifests::Perturbation> perturbations) {
    manifests::ChaosManifest manifest;
    manifest.test_name = "test";
    manifest.target = manifests::Target{"target-container"};
    manifest.perturbations = std::move(perturbations);
    return manifest;
}

}  // namespace

/**
 * @test Verifies applyAll applies perturbations in manifest order.
 */
TEST(PerturbationEngineUnitTest, ApplyAllAppliesPerturbationsInOrder) {
    auto mockEngine = std::make_shared<testing::NiceMock<tests::MockContainerEngine>>();

    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 0, 0, 0)).Times(AnyNumber());

    testing::Sequence seq;
    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 524288000, 0, 0))
        .Times(1)
        .InSequence(seq);
    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 1048576, 0, 0))
        .Times(1)
        .InSequence(seq);

    manifests::Perturbation mem1Spec{"memory_cap", {{"limit_bytes", "524288000"}}};
    manifests::Perturbation mem2Spec{"memory_cap", {{"limit_bytes", "1048576"}}};

    perturbations::PerturbationEngine engine(mockEngine);
    engine.applyAll(makeManifest({mem1Spec, mem2Spec}));
}

/**
 * @test Verifies applyAll stops and does not revert when apply throws.
 */
TEST(PerturbationEngineUnitTest, ApplyAllSkipsTrackingWhenApplyThrows) {
    auto mockEngine = std::make_shared<testing::NiceMock<tests::MockContainerEngine>>();

    testing::InSequence seq;
    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 1048576, 0, 0)).Times(1);
    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 0, 0, 0)).Times(1);

    manifests::Perturbation memSpec{"memory_cap", {{"limit_bytes", "1048576"}}};
    manifests::Perturbation cpuSpec{"cpu_cap", {}};

    perturbations::PerturbationEngine engine(mockEngine);
    EXPECT_THROW(engine.applyAll(makeManifest({memSpec, cpuSpec})), std::invalid_argument);
}

/**
 * @test Verifies revertAll reverts perturbations in LIFO order.
 */
TEST(PerturbationEngineUnitTest, RevertAllRevertsInReverseOrder) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();

    testing::InSequence seq;
    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 524288000, 0, 0)).Times(1);
    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 1048576, 0, 0)).Times(1);
    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 0, 0, 0)).Times(2);

    manifests::Perturbation mem1Spec{"memory_cap", {{"limit_bytes", "524288000"}}};
    manifests::Perturbation mem2Spec{"memory_cap", {{"limit_bytes", "1048576"}}};

    perturbations::PerturbationEngine engine(mockEngine);
    engine.applyAll(makeManifest({mem1Spec, mem2Spec}));
    engine.revertAll();
}

/**
 * @test Verifies revertAll is a no-op when nothing was applied.
 */
TEST(PerturbationEngineUnitTest, RevertAllIsNoOpWhenNothingApplied) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, startContainer(_)).Times(0);
    EXPECT_CALL(*mockEngine, updateResources(_, _, _, _)).Times(0);

    perturbations::PerturbationEngine engine(mockEngine);
    EXPECT_NO_THROW(engine.revertAll());
}

/**
 * @test Verifies destructor reverts any applied perturbations.
 */
TEST(PerturbationEngineUnitTest, DestructorRevertsAppliedPerturbations) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();

    testing::InSequence seq;
    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 1048576, 0, 0)).Times(1);
    EXPECT_CALL(*mockEngine, updateResources(std::string_view("target-container"), 0, 0, 0)).Times(1);

    manifests::Perturbation memSpec{"memory_cap", {{"limit_bytes", "1048576"}}};

    {
        perturbations::PerturbationEngine engine(mockEngine);
        engine.applyAll(makeManifest({memSpec}));
    }
}
