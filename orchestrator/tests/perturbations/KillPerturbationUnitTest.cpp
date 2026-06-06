/**
 * @file KillPerturbationUnitTest.cpp
 * @brief Unit tests for KillPerturbation apply/revert edge cases.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "perturbations/internal/KillPerturbation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::perturbations;
using namespace testing;

/**
 * @test Verifies revert does not clear the applied flag when the start call fails.
 */
TEST(KillPerturbationTest, RevertDoesNotClearAppliedFlagOnFailure) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    EXPECT_CALL(*engine, killContainer(std::string_view("test-id"))).Times(1);

    KillPerturbation p(engine, "test-id");
    p.apply();

    EXPECT_CALL(*engine, startContainer(std::string_view("test-id")))
        .WillOnce(Throw(containers::ContainerEngineError("start failed")));

    bool revertThrew = false;
    try {
        p.revert();
    } catch (const std::system_error&) {
        revertThrew = true;
    }
    EXPECT_TRUE(revertThrew);

    EXPECT_CALL(*engine, startContainer(std::string_view("test-id"))).Times(1);

    EXPECT_NO_THROW(p.revert());
}

/**
 * @test Verifies apply is idempotent (second call is a no-op).
 */
TEST(KillPerturbationTest, ApplyIsIdempotent) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    EXPECT_CALL(*engine, killContainer(std::string_view("test-id"))).Times(1);

    KillPerturbation p(engine, "test-id");
    p.apply();
    p.apply();
}

/**
 * @test Verifies revert is a no-op when apply has not been called.
 */
TEST(KillPerturbationTest, RevertIsNoOpBeforeApply) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    EXPECT_CALL(*engine, startContainer(_)).Times(0);

    KillPerturbation p(engine, "test-id");
    EXPECT_NO_THROW(p.revert());
}
