/**
 * @file PacketFloodPerturbationUnitTest.cpp
 * @brief Unit tests for PacketFloodPerturbation apply/revert edge cases.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "perturbations/internal/PacketFloodPerturbation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::perturbations;
using namespace testing;

/**
 * @test Verifies apply retrieves container network namespace details for socket setup.
 */
TEST(PacketFloodPerturbationTest, ApplyRetrievesNetnsAndPid) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"packet_flood", {}};

    EXPECT_CALL(*engine, getContainerIp(std::string_view("target"))).WillOnce(Return("10.0.0.1"));
    EXPECT_CALL(*engine, getContainerPid(std::string_view("target"))).Times(1).WillOnce(Return(12345));
    EXPECT_CALL(*engine, getContainerNetnsFd(std::string_view("target"))).Times(1).WillOnce(Return(42));

    PacketFloodPerturbation p(engine, "target", spec);

    EXPECT_THROW(p.apply(), std::system_error);
}

/**
 * @test Verifies revert is a no-op when apply failed and the flood thread was never started.
 */
TEST(PacketFloodPerturbationTest, RevertIsNoOpAfterFailedApply) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"packet_flood", {}};

    EXPECT_CALL(*engine, getContainerIp(std::string_view("target"))).WillOnce(Return("10.0.0.1"));
    EXPECT_CALL(*engine, getContainerPid(std::string_view("target"))).WillOnce(Return(12345));
    EXPECT_CALL(*engine, getContainerNetnsFd(std::string_view("target"))).WillOnce(Return(42));

    PacketFloodPerturbation p(engine, "target", spec);

    EXPECT_THROW(p.apply(), std::system_error);

    EXPECT_NO_THROW(p.revert());
}

/**
 * @test Verifies apply throws when container PID is invalid (<= 0).
 */
TEST(PacketFloodPerturbationTest, ApplyThrowsOnInvalidPid) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"packet_flood", {}};

    EXPECT_CALL(*engine, getContainerPid(std::string_view("target"))).WillOnce(Return(0));

    PacketFloodPerturbation p(engine, "target", spec);

    EXPECT_THROW(p.apply(), std::system_error);
}
