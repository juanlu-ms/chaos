/**
 * @file NetworkCutoffPerturbationUnitTest.cpp
 * @brief Unit tests for NetworkCutoffPerturbation apply/revert edge cases.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "perturbations/internal/NetworkCutoffPerturbation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::perturbations;
using namespace testing;

/**
 * @test Verifies revert does not clear the applied flag when a deletion fails.
 */
TEST(NetworkCutoffPerturbationTest, RevertDoesNotClearAppliedFlagOnFailure) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_cutoff", {}};

    testing::InSequence seq;
    EXPECT_CALL(*engine, execInNetNs(std::string_view("target"), std::string_view("iptables -A OUTPUT -j DROP")))
        .WillOnce(Return(std::string{}));
    EXPECT_CALL(*engine, execInNetNs(std::string_view("target"), std::string_view("iptables -A INPUT -j DROP")))
        .WillOnce(Return(std::string{}));

    NetworkCutoffPerturbation p(engine, "target", spec);
    p.apply();

    EXPECT_CALL(*engine, execInNetNs(std::string_view("target"), std::string_view("iptables -D OUTPUT -j DROP")))
        .WillOnce(Throw(containers::ContainerEngineError("exec failed")));

    bool revertThrew = false;
    try {
        p.revert();
    } catch (const std::system_error&) {
        revertThrew = true;
    }
    EXPECT_TRUE(revertThrew);

    EXPECT_CALL(*engine, execInNetNs(std::string_view("target"), std::string_view("iptables -D OUTPUT -j DROP")))
        .WillOnce(Return(std::string{}));
    EXPECT_CALL(*engine, execInNetNs(std::string_view("target"), std::string_view("iptables -D INPUT -j DROP")))
        .WillOnce(Return(std::string{}));

    EXPECT_NO_THROW(p.revert());
}

/**
 * @test Verifies apply is idempotent (second call is a no-op after first apply).
 */
TEST(NetworkCutoffPerturbationTest, ApplyIsIdempotent) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    manifests::Perturbation spec{"network_cutoff", {}};

    EXPECT_CALL(*engine, execInNetNs(_, _)).Times(2).WillRepeatedly(Return(std::string{}));

    NetworkCutoffPerturbation p(engine, "target", spec);
    p.apply();
    p.apply();
}
