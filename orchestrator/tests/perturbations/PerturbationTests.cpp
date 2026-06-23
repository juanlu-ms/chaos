/**
 * @file PerturbationTests.cpp
 * @brief Unit tests for concrete perturbations and perturbation factory behavior.
 */

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "MockContainerEngine.hpp"
#include "containers/SystemInfo.hpp"
#include "manifests/Manifest.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "perturbations/internal/CpuCapPerturbation.hpp"
#include "perturbations/internal/KillPerturbation.hpp"
#include "perturbations/internal/MemoryCapPerturbation.hpp"
#include "perturbations/internal/NetworkCutoffPerturbation.hpp"
#include "perturbations/internal/NetworkDelayPerturbation.hpp"
#include "perturbations/internal/PacketFloodPerturbation.hpp"
#include "perturbations/internal/TrafficCorruptionPerturbation.hpp"

using namespace testing;
using namespace chaos::orchestrator;

/**
 * @test Verifies KillPerturbation delegates to killContainer for a valid target.
 */
TEST(PerturbationTests, KillPerturbationCallsKillContainer) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    EXPECT_CALL(*mock_engine, killContainer(std::string_view("test-container"))).Times(1);

    perturbations::KillPerturbation kill_pert(mock_engine, target.id);
    kill_pert.apply();
}

/**
 * @test Verifies KillPerturbation::revert delegates to startContainer after apply.
 */
TEST(PerturbationTests, KillPerturbationRevertCallsStartContainer) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    testing::InSequence seq;
    EXPECT_CALL(*mock_engine, killContainer(std::string_view("test-container"))).Times(1);
    EXPECT_CALL(*mock_engine, startContainer(std::string_view("test-container"))).Times(1);

    perturbations::KillPerturbation pert(mock_engine, target.id);
    pert.apply();
    pert.revert();
}

/**
 * @test Verifies KillPerturbation throws when target name is empty.
 */
TEST(PerturbationTests, PerturbationThrowsOnEmptyTarget) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{""};

    perturbations::KillPerturbation kill_pert(mock_engine, target.id);
    EXPECT_THROW(kill_pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies factory returns supported perturbation implementations and rejects unknown types.
 */
TEST(PerturbationTests, FactoryCreatesProperInstances) {
    perturbations::PerturbationFactory factory;
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    manifests::Perturbation killSpec{"kill", {}};
    auto kill_pert = factory.create(mock_engine, target, killSpec);
    EXPECT_NE(dynamic_cast<perturbations::KillPerturbation*>(kill_pert.get()), nullptr);

    manifests::Perturbation memSpec{"memory_cap", {{"limit_bytes", "50M"}}};
    auto mem_pert = factory.create(mock_engine, target, memSpec);
    EXPECT_NE(dynamic_cast<perturbations::MemoryCapPerturbation*>(mem_pert.get()), nullptr);

    manifests::Perturbation netSpec{"network_delay", {{"delay_ms", "120"}}};
    auto net_pert = factory.create(mock_engine, target, netSpec);
    EXPECT_NE(dynamic_cast<perturbations::NetworkDelayPerturbation*>(net_pert.get()), nullptr);

    EXPECT_THROW(factory.create(mock_engine, target, {"unknown", {}}), std::invalid_argument);
}

/**
 * @test Verifies MemoryCapPerturbation throws when required limit_bytes parameter is missing.
 */
TEST(PerturbationTests, MemoryCapThrowsOnMissingParameter) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};
    manifests::Perturbation spec{"memory_cap", {}};

    perturbations::MemoryCapPerturbation pert(mock_engine, target.id, spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies CpuCapPerturbation throws when required quota parameter is missing.
 */
TEST(PerturbationTests, CpuCapThrowsOnMissingParameter) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};
    manifests::Perturbation spec{"cpu_cap", {}};

    perturbations::CpuCapPerturbation pert(mock_engine, target.id, spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies NetworkDelayPerturbation throws when required delay_ms parameter is missing.
 */
TEST(PerturbationTests, NetworkDelayThrowsOnMissingParameter) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};
    manifests::Perturbation spec{"network_delay", {}};

    perturbations::NetworkDelayPerturbation pert(mock_engine, target.id, spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies CpuCapPerturbation::revert is idempotent when not applied.
 */
TEST(PerturbationTests, CpuCapRevertIsNoOpWhenNotApplied) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"cpu_cap", {{"cpu_cores", "2"}}};

    perturbations::CpuCapPerturbation pert(mock_engine, "test-container", spec);
    // Should not throw — revert is a no-op when not applied
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies MemoryCapPerturbation::revert is idempotent when not applied.
 */
TEST(PerturbationTests, MemoryCapRevertIsNoOpWhenNotApplied) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"memory_cap", {{"limit_bytes", "104857600"}}};

    perturbations::MemoryCapPerturbation pert(mock_engine, "test-container", spec);
    // Should not throw — revert is a no-op when not applied
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies CpuCapPerturbation::apply is idempotent when called twice.
 * @note This test verifies the guard flag prevents re-applying.
 */
TEST(PerturbationTests, CpuCapApplyIsSkippedWhenAlreadyApplied) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"cpu_cap", {{"cpu_cores", "2"}}};

    EXPECT_CALL(*mock_engine, updateCpuQuota(std::string_view("target"), 200000, 100000)).Times(1);

    perturbations::CpuCapPerturbation pert(mock_engine, "target", spec);
    pert.apply();
    pert.apply();
}

/**
 * @test Verifies MemoryCapPerturbation::apply throws on missing limit_bytes.
 */
TEST(PerturbationTests, MemoryCapThrowsOnMissingLimitBytes) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"memory_cap", {}};  // No limit_bytes

    perturbations::MemoryCapPerturbation pert(mock_engine, "test-container", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies CpuCapPerturbation::apply throws when target ID is empty.
 */
TEST(PerturbationTests, CpuCapThrowsOnEmptyTargetId) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"cpu_cap", {{"cpu_cores", "2"}}};

    perturbations::CpuCapPerturbation pert(mock_engine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies MemoryCapPerturbation::apply throws when target ID is empty.
 */
TEST(PerturbationTests, MemoryCapThrowsOnEmptyTargetId) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"memory_cap", {{"limit_bytes", "104857600"}}};

    perturbations::MemoryCapPerturbation pert(mock_engine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies the factory creates NetworkCutoffPerturbation for type "network_cutoff".
 */
TEST(PerturbationTests, FactoryCreatesNetworkCutoffPerturbation) {
    perturbations::PerturbationFactory factory;
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    manifests::Perturbation spec{"network_cutoff", {{"dst_ip", "8.8.8.8"}}};
    auto pert = factory.create(mock_engine, target, spec);
    EXPECT_NE(dynamic_cast<perturbations::NetworkCutoffPerturbation*>(pert.get()), nullptr);
}

/**
 * @test Verifies the factory creates TrafficCorruptionPerturbation for type "traffic_corruption".
 */
TEST(PerturbationTests, FactoryCreatesTrafficCorruptionPerturbation) {
    perturbations::PerturbationFactory factory;
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    manifests::Perturbation spec{"traffic_corruption", {{"corrupt_pct", "5%"}}};
    auto pert = factory.create(mock_engine, target, spec);
    EXPECT_NE(dynamic_cast<perturbations::TrafficCorruptionPerturbation*>(pert.get()), nullptr);
}

/**
 * @test Verifies the factory creates PacketFloodPerturbation for type "packet_flood".
 */
TEST(PerturbationTests, FactoryCreatesPacketFloodPerturbation) {
    perturbations::PerturbationFactory factory;
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Target target{"test-container"};

    manifests::Perturbation spec{"packet_flood", {}};
    auto pert = factory.create(mock_engine, target, spec);
    EXPECT_NE(dynamic_cast<perturbations::PacketFloodPerturbation*>(pert.get()), nullptr);
}

/**
 * @test Verifies NetworkCutoffPerturbation::revert is a no-op when not applied.
 */
TEST(PerturbationTests, NetworkCutoffRevertIsNoOpWhenNotApplied) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_cutoff", {{"dst_ip", "8.8.8.8"}}};

    perturbations::NetworkCutoffPerturbation pert(mock_engine, "test-container", spec);
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies NetworkCutoffPerturbation::apply throws when target ID is empty.
 */
TEST(PerturbationTests, NetworkCutoffThrowsOnEmptyTargetId) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_cutoff", {{"dst_ip", "8.8.8.8"}}};

    perturbations::NetworkCutoffPerturbation pert(mock_engine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies TrafficCorruptionPerturbation::revert is a no-op when not applied.
 */
TEST(PerturbationTests, TrafficCorruptionRevertIsNoOpWhenNotApplied) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"traffic_corruption", {{"corrupt_pct", "5%"}}};

    perturbations::TrafficCorruptionPerturbation pert(mock_engine, "test-container", spec);
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies TrafficCorruptionPerturbation::revert calls exec with the correct delete command after apply.
 */
TEST(PerturbationTests, TrafficCorruptionRevertCallsExecWithDeleteCommand) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"traffic_corruption", {{"corrupt_pct", "5%"}}};

    testing::InSequence seq;
    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target"),
                                          std::string_view("tc qdisc replace dev eth0 root netem corrupt 5%")))
        .WillOnce(Return(std::string{}));
    EXPECT_CALL(*mock_engine,
                execInNetNs(std::string_view("target"), std::string_view("tc qdisc del dev eth0 root netem")))
        .WillOnce(Return(std::string{}));

    perturbations::TrafficCorruptionPerturbation pert(mock_engine, "target", spec);
    pert.apply();
    pert.revert();
}

/**
 * @test Verifies TrafficCorruptionPerturbation::apply throws when target ID is empty.
 */
TEST(PerturbationTests, TrafficCorruptionThrowsOnEmptyTargetId) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"traffic_corruption", {{"corrupt_pct", "5%"}}};

    perturbations::TrafficCorruptionPerturbation pert(mock_engine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

/**
 * @test Verifies NetworkDelayPerturbation::apply calls exec with the correct tc netem command.
 *
 * Now that NetworkDelayPerturbation delegates to IContainerEngine::execInNetNs, it can be
 * fully verified through the mock without any real Docker dependency.
 */
TEST(PerturbationTests, NetworkDelayApplyCallsExecWithCorrectCommand) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_delay", {{"delay_ms", "100"}}};

    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target-c"),
                                          std::string_view("tc qdisc replace dev eth0 root netem delay 100ms")))
        .Times(1)
        .WillOnce(Return(std::string{}));

    perturbations::NetworkDelayPerturbation pert(mock_engine, "target-c", spec);
    EXPECT_NO_THROW(pert.apply());
}

/**
 * @test Verifies NetworkDelayPerturbation::revert calls exec with the correct tc delete command.
 */
TEST(PerturbationTests, NetworkDelayRevertCallsExecWithCorrectCommand) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_delay", {{"delay_ms", "50"}}};

    testing::InSequence seq;
    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target-c"),
                                          std::string_view("tc qdisc replace dev eth0 root netem delay 50ms")))
        .WillOnce(Return(std::string{}));
    EXPECT_CALL(*mock_engine,
                execInNetNs(std::string_view("target-c"), std::string_view("tc qdisc del dev eth0 root netem")))
        .WillOnce(Return(std::string{}));

    perturbations::NetworkDelayPerturbation pert(mock_engine, "target-c", spec);
    pert.apply();
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies NetworkDelayPerturbation::apply is idempotent (second call is a no-op).
 */
TEST(PerturbationTests, NetworkDelayApplyIsSkippedWhenAlreadyApplied) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_delay", {{"delay_ms", "100"}}};

    // exec should be called only once despite two apply() calls
    EXPECT_CALL(*mock_engine, execInNetNs(::testing::_, ::testing::_)).Times(1).WillOnce(Return(std::string{}));

    perturbations::NetworkDelayPerturbation pert(mock_engine, "target-c", spec);
    pert.apply();
    pert.apply();  // second call should be no-op
}

/**
 * @test Verifies NetworkDelayPerturbation::revert is a no-op when not applied.
 */
TEST(PerturbationTests, NetworkDelayRevertIsNoOpWhenNotApplied) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_delay", {{"delay_ms", "100"}}};

    EXPECT_CALL(*mock_engine, execInNetNs(::testing::_, ::testing::_)).Times(0);

    perturbations::NetworkDelayPerturbation pert(mock_engine, "target-c", spec);
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies NetworkDelayPerturbation::apply throws when empty target ID.
 */
TEST(PerturbationTests, NetworkDelayThrowsOnEmptyTargetId) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_delay", {{"delay_ms", "100"}}};

    perturbations::NetworkDelayPerturbation pert(mock_engine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

TEST(PerturbationTests, CpuCapApplyCallsUpdateResourcesWithCorrectCpuCores) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"cpu_cap", {{"cpu_cores", "2"}}};
    EXPECT_CALL(*mock_engine, updateCpuQuota(std::string_view("target"), 200000, 100000)).Times(1);
    perturbations::CpuCapPerturbation pert(mock_engine, "target", spec);
    EXPECT_NO_THROW(pert.apply());
}

TEST(PerturbationTests, MemoryCapApplyCallsUpdateResourcesWithCorrectLimit) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"memory_cap", {{"limit_bytes", "104857600"}}};
    EXPECT_CALL(*mock_engine, updateMemoryLimit(std::string_view("target"), 104857600)).Times(1);
    perturbations::MemoryCapPerturbation pert(mock_engine, "target", spec);
    EXPECT_NO_THROW(pert.apply());
}

/**
 * @test Verifies TrafficCorruptionPerturbation::apply uses default corrupt 100 when no parameters provided.
 */
TEST(PerturbationTests, TrafficCorruptionApplyCallsExecWithDefaultNetemCommand) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"traffic_corruption", {}};
    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target"),
                                          std::string_view("tc qdisc replace dev eth0 root netem corrupt 100")))
        .Times(1)
        .WillOnce(Return(std::string{}));
    perturbations::TrafficCorruptionPerturbation pert(mock_engine, "target", spec);
    EXPECT_NO_THROW(pert.apply());
}

/**
 * @test Verifies PacketFloodPerturbation::revert is a no-op when not applied.
 */
TEST(PerturbationTests, PacketFloodRevertIsNoOpWhenNotApplied) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"packet_flood", {{"iface", "eth0"}}};

    perturbations::PacketFloodPerturbation pert(mock_engine, "test-container", spec);
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies PacketFloodPerturbation::apply throws when target ID is empty.
 */
TEST(PerturbationTests, PacketFloodThrowsOnEmptyTargetId) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"packet_flood", {}};

    perturbations::PacketFloodPerturbation pert(mock_engine, "", spec);
    EXPECT_THROW(pert.apply(), std::invalid_argument);
}

TEST(PerturbationTests, NetworkCutoffApplyCallsExecWithIptablesDropRules) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_cutoff", {}};

    testing::InSequence seq;
    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target"), std::string_view("iptables -A OUTPUT -j DROP")))
        .WillOnce(Return(std::string{}));
    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target"), std::string_view("iptables -A INPUT -j DROP")))
        .WillOnce(Return(std::string{}));

    perturbations::NetworkCutoffPerturbation pert(mock_engine, "target", spec);
    EXPECT_NO_THROW(pert.apply());
}

TEST(PerturbationTests, CpuCapRevertCallsUpdateResourcesWithZeroCpuCores) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"cpu_cap", {{"cpu_cores", "2"}}};

    testing::InSequence seq;
    EXPECT_CALL(*mock_engine, updateCpuQuota(std::string_view("target"), 200000, 100000)).Times(1);
    EXPECT_CALL(*mock_engine, updateCpuQuota(std::string_view("target"), -1, 100000)).Times(1);

    perturbations::CpuCapPerturbation pert(mock_engine, "target", spec);
    pert.apply();
    EXPECT_NO_THROW(pert.revert());
}

TEST(PerturbationTests, NetworkCutoffRevertCallsExecWithDeleteRules) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_cutoff", {}};

    testing::InSequence seq;
    // apply() calls addRule(OUTPUT first, then INPUT) — each calls exec with "iptables -A ..."
    // revertCommands_ is populated in same order, so revert() calls "iptables -D OUTPUT ..." then "iptables -D INPUT
    // ..."
    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target"), std::string_view("iptables -A OUTPUT -j DROP")))
        .WillOnce(Return(std::string{}));
    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target"), std::string_view("iptables -A INPUT -j DROP")))
        .WillOnce(Return(std::string{}));
    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target"), std::string_view("iptables -D OUTPUT -j DROP")))
        .WillOnce(Return(std::string{}));
    EXPECT_CALL(*mock_engine, execInNetNs(std::string_view("target"), std::string_view("iptables -D INPUT -j DROP")))
        .WillOnce(Return(std::string{}));

    perturbations::NetworkCutoffPerturbation pert(mock_engine, "target", spec);
    pert.apply();
    EXPECT_NO_THROW(pert.revert());
}

/**
 * @test Verifies MemoryCapPerturbation::revert restores memory to total system memory.
 */
TEST(PerturbationTests, MemoryCapRevertUsesTotalSystemMemory) {
    auto mock_engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"memory_cap", {{"limit_bytes", "104857600"}}};

    containers::SystemInfo sys_info{8589934592};  // 8 GiB

    testing::InSequence seq;
    EXPECT_CALL(*mock_engine, getSystemInfo()).Times(1).WillOnce(Return(sys_info));
    EXPECT_CALL(*mock_engine, updateMemoryLimit(std::string_view("target"), 104857600)).Times(1);
    EXPECT_CALL(*mock_engine, updateMemoryLimit(std::string_view("target"), 8589934592)).Times(1);

    perturbations::MemoryCapPerturbation pert(mock_engine, "target", spec);
    pert.apply();
    pert.revert();
}
