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

    EXPECT_CALL(*engine, getContainerPid(std::string_view("target"))).Times(1).WillOnce(Return(12345));
    EXPECT_CALL(*engine, getContainerNetnsFd(std::string_view("target"))).Times(1).WillOnce(Return(42));

    PacketFloodPerturbation p(engine, "target", spec);

    EXPECT_THROW(p.apply(), std::system_error);
}

/**
 * @test Verifies revert signals stop to the flood thread.
 */
TEST(PacketFloodPerturbationTest, RevertSignalsStop) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"packet_flood", {}};

    EXPECT_CALL(*engine, getContainerPid(std::string_view("target"))).WillOnce(Return(12345));
    EXPECT_CALL(*engine, getContainerNetnsFd(std::string_view("target"))).WillOnce(Return(42));

    PacketFloodPerturbation p(engine, "target", spec);

    EXPECT_THROW(p.apply(), std::system_error);

    EXPECT_NO_THROW(p.revert());
}
