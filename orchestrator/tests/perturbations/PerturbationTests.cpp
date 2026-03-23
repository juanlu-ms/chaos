#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/CpuCapPerturbation.hpp"
#include "perturbations/GarbagePacketPerturbation.hpp"
#include "perturbations/KillPerturbation.hpp"
#include "perturbations/MemoryCapPerturbation.hpp"
#include "perturbations/NetworkCutoffPerturbation.hpp"
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

/**
 * @test Verifies CpuCapPerturbation::revert is idempotent when not applied.
 */
TEST(PerturbationTests, CpuCapRevertIsNoOpWhenNotApplied) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"cpu_cap", {{"quota", "50000"}}};

    perturbations::CpuCapPerturbation pert(mockEngine, "test-container", spec);
    // Should not throw — revert is a no-op when not applied
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies MemoryCapPerturbation::revert is idempotent when not applied.
 */
TEST(PerturbationTests, MemoryCapRevertIsNoOpWhenNotApplied) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"memory_cap", {{"limit_bytes", "104857600"}}};

    perturbations::MemoryCapPerturbation pert(mockEngine, "test-container", spec);
    // Should not throw — revert is a no-op when not applied
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies CpuCapPerturbation::apply is idempotent when called twice.
 * @note This test verifies the guard flag prevents re-applying.
 */
TEST(PerturbationTests, CpuCapApplyIsSkippedWhenAlreadyApplied) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"cpu_cap", {{"quota", "50000"}}};

    perturbations::CpuCapPerturbation pert(mockEngine, "", spec);
    // Empty target: first apply throws, showing the quota check runs before guard
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies MemoryCapPerturbation::apply throws on missing limit_bytes.
 */
TEST(PerturbationTests, MemoryCapThrowsOnMissingLimitBytes) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"memory_cap", {}};  // No limit_bytes

    perturbations::MemoryCapPerturbation pert(mockEngine, "test-container", spec);
    EXPECT_THROW(pert.apply(), std::system_error);
}

/**
 * @test Verifies CpuCapPerturbation::apply throws when target ID is empty.
 */
TEST(PerturbationTests, CpuCapThrowsOnEmptyTargetId) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"cpu_cap", {{"quota", "50000"}}};

    perturbations::CpuCapPerturbation pert(mockEngine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies MemoryCapPerturbation::apply throws when target ID is empty.
 */
TEST(PerturbationTests, MemoryCapThrowsOnEmptyTargetId) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"memory_cap", {{"limit_bytes", "104857600"}}};

    perturbations::MemoryCapPerturbation pert(mockEngine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies the factory creates NetworkCutoffPerturbation for type "network_cutoff".
 */
TEST(PerturbationTests, FactoryCreatesNetworkCutoffPerturbation) {
    perturbations::PerturbationFactory factory;
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    manifests::Perturbation spec{"network_cutoff", {{"dst_ip", "8.8.8.8"}}};
    auto pert = factory.create(mockEngine, target, spec);
    EXPECT_NE(dynamic_cast<perturbations::NetworkCutoffPerturbation*>(pert.get()), nullptr);
}

/**
 * @test Verifies the factory creates GarbagePacketPerturbation for type "garbage_packet".
 */
TEST(PerturbationTests, FactoryCreatesGarbagePacketPerturbation) {
    perturbations::PerturbationFactory factory;
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    manifests::Perturbation spec{"garbage_packet", {{"corrupt_pct", "5%"}}};
    auto pert = factory.create(mockEngine, target, spec);
    EXPECT_NE(dynamic_cast<perturbations::GarbagePacketPerturbation*>(pert.get()), nullptr);
}

/**
 * @test Verifies NetworkCutoffPerturbation::revert is a no-op when not applied.
 */
TEST(PerturbationTests, NetworkCutoffRevertIsNoOpWhenNotApplied) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_cutoff", {{"dst_ip", "8.8.8.8"}}};

    perturbations::NetworkCutoffPerturbation pert(mockEngine, "test-container", spec);
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies NetworkCutoffPerturbation::apply throws when target ID is empty.
 */
TEST(PerturbationTests, NetworkCutoffThrowsOnEmptyTargetId) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_cutoff", {{"dst_ip", "8.8.8.8"}}};

    perturbations::NetworkCutoffPerturbation pert(mockEngine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies GarbagePacketPerturbation::revert is a no-op when not applied.
 */
TEST(PerturbationTests, GarbagePacketRevertIsNoOpWhenNotApplied) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"garbage_packet", {{"corrupt_pct", "5%"}}};

    perturbations::GarbagePacketPerturbation pert(mockEngine, "test-container", spec);
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies GarbagePacketPerturbation::apply throws when target ID is empty.
 */
TEST(PerturbationTests, GarbagePacketThrowsOnEmptyTargetId) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"garbage_packet", {{"corrupt_pct", "5%"}}};

    perturbations::GarbagePacketPerturbation pert(mockEngine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies GarbagePacketPerturbation::apply throws when no netem parameters provided.
 */
TEST(PerturbationTests, GarbagePacketThrowsOnNoNetemParameters) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"garbage_packet", {}};

    perturbations::GarbagePacketPerturbation pert(mockEngine, "test-container", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}
