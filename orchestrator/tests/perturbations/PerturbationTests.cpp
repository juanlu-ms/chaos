#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/CpuCapPerturbation.hpp"
#include "perturbations/KillPerturbation.hpp"
#include "perturbations/MemoryCapPerturbation.hpp"
#include "perturbations/NetworkDelayPerturbation.hpp"
#include "perturbations/PerturbationFactory.hpp"

/**
 * @file PerturbationTests.cpp
 * @brief Unit tests for concrete perturbations and perturbation factory behavior.
 */

using namespace testing;
using namespace chaos::orchestrator;

/**
 * @test Verifies KillPerturbation delegates to killContainer for a valid target.
 */
TEST(PerturbationTests, KillPerturbationCallsKillContainer) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    EXPECT_CALL(*mockEngine, killContainer(std::string_view("test-container"))).Times(1);

    perturbations::KillPerturbation killPert(mockEngine, target.id);
    killPert.apply();
}

/**
 * @test Verifies KillPerturbation throws when target name is empty.
 */
TEST(PerturbationTests, PerturbationThrowsOnEmptyTarget) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{""};

    perturbations::KillPerturbation killPert(mockEngine, target.id);
    EXPECT_THROW(killPert.apply(), std::invalid_argument);
}

/**
 * @test Verifies factory returns supported perturbation implementations and rejects unknown types.
 */
TEST(PerturbationTests, FactoryCreatesProperInstances) {
    perturbations::PerturbationFactory factory;
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    manifests::Perturbation killSpec{"kill", {}};
    auto killPert = factory.create(mockEngine, target, killSpec);
    EXPECT_NE(dynamic_cast<perturbations::KillPerturbation*>(killPert.get()), nullptr);

    manifests::Perturbation memSpec{"memory_cap", {{"limit_bytes", "50M"}}};
    auto memPert = factory.create(mockEngine, target, memSpec);
    EXPECT_NE(dynamic_cast<perturbations::MemoryCapPerturbation*>(memPert.get()), nullptr);

    manifests::Perturbation netSpec{"network_delay", {{"delay_ms", "120"}}};
    auto netPert = factory.create(mockEngine, target, netSpec);
    EXPECT_NE(dynamic_cast<perturbations::NetworkDelayPerturbation*>(netPert.get()), nullptr);

    EXPECT_THROW(factory.create(mockEngine, target, {"unknown", {}}), std::invalid_argument);
}

/**
 * @test Verifies MemoryCapPerturbation throws when required limit_bytes parameter is missing.
 */
TEST(PerturbationTests, MemoryCapThrowsOnMissingParameter) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};
    manifests::Perturbation spec{"memory_cap", {}};

    perturbations::MemoryCapPerturbation pert(mockEngine, target.id, spec);
    EXPECT_THROW(pert.apply(), std::system_error);
}

/**
 * @test Verifies CpuCapPerturbation throws when required quota parameter is missing.
 */
TEST(PerturbationTests, CpuCapThrowsOnMissingParameter) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};
    manifests::Perturbation spec{"cpu_cap", {}};

    perturbations::CpuCapPerturbation pert(mockEngine, target.id, spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies NetworkDelayPerturbation throws when required delay_ms parameter is missing.
 */
TEST(PerturbationTests, NetworkDelayThrowsOnMissingParameter) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};
    manifests::Perturbation spec{"network_delay", {}};

    perturbations::NetworkDelayPerturbation pert(mockEngine, target.id, spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}
