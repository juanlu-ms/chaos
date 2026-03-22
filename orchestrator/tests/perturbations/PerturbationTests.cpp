#include <gtest/gtest.h>

#include "MockContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/CpuCapPerturbation.hpp"
#include "perturbations/KillPerturbation.hpp"
#include "perturbations/MemoryCapPerturbation.hpp"
#include "perturbations/NetworkCapPerturbation.hpp"
#include "perturbations/PerturbationFactory.hpp"

using namespace testing;
using namespace chaos::orchestrator;

TEST(PerturbationTests, KillPerturbationCallsKillContainer) {
    tests::MockContainerEngine mockEngine;
    manifests::Target target{"container", "test-container"};

    EXPECT_CALL(mockEngine, killContainer(std::string_view("test-container"))).Times(1);

    perturbations::KillPerturbation killPert;
    killPert.apply(mockEngine, target);
}

TEST(PerturbationTests, PerturbationThrowsOnEmptyTarget) {
    tests::MockContainerEngine mockEngine;
    manifests::Target target{"container", ""};

    perturbations::KillPerturbation killPert;
    EXPECT_THROW(killPert.apply(mockEngine, target), std::system_error);
}

TEST(PerturbationTests, FactoryCreatesProperInstances) {
    perturbations::PerturbationFactory factory;

    manifests::Perturbation killSpec{"kill", {}};
    auto killPert = factory.create(killSpec);
    EXPECT_NE(dynamic_cast<perturbations::KillPerturbation*>(killPert.get()), nullptr);

    manifests::Perturbation memSpec{"memory_cap", {{"limit_bytes", "50M"}}};
    auto memPert = factory.create(memSpec);
    EXPECT_NE(dynamic_cast<perturbations::NetworkDelayPerturbation*>(memPert.get()), nullptr);

    EXPECT_THROW(factory.create({"unknown", {}}), std::invalid_argument);
}

TEST(PerturbationTests, MemoryCapThrowsOnMissingParameter) {
    tests::MockContainerEngine mockEngine;
    manifests::Target target{"container", "test-container"};

    perturbations::NetworkDelayPerturbation pert({});
    EXPECT_THROW(pert.apply(mockEngine, target), std::system_error);
}

TEST(PerturbationTests, CpuCapThrowsOnMissingParameter) {
    tests::MockContainerEngine mockEngine;
    manifests::Target target{"container", "test-container"};

    perturbations::CpuCapPerturbation pert({});
    EXPECT_THROW(pert.apply(mockEngine, target), std::system_error);
}

TEST(PerturbationTests, NetworkCapThrowsOnMissingParameter) {
    tests::MockContainerEngine mockEngine;
    manifests::Target target{"container", "test-container"};

    perturbations::NetworkCapPerturbation pert({});
    EXPECT_THROW(pert.apply(mockEngine, target), std::system_error);
}
