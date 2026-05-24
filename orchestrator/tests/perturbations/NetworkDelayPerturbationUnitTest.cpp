/**
 * @file NetworkDelayPerturbationUnitTest.cpp
 * @brief Unit tests for NetworkDelayPerturbation revert-on-failure behavior.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "MockContainerEngine.hpp"
#include "perturbations/internal/NetworkDelayPerturbation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::perturbations;
using namespace testing;

/**
 * @test Verifies revert does not clear the applied flag when the initial revert attempt fails.
 */
TEST(NetworkDelayPerturbationTest, RevertDoesNotClearAppliedFlagOnFailure) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    manifests::Perturbation spec;
    spec.type = "network_delay";
    spec.parameters["delay_ms"] = "200";

    EXPECT_CALL(*engine, execInNetNs("test-id", _)).WillOnce(Return(std::string{}));

    NetworkDelayPerturbation p(engine, "test-id", spec);
    p.apply();

    EXPECT_CALL(*engine, execInNetNs("test-id", _)).WillOnce(Throw(containers::ContainerEngineError("tc failed")));

    bool revertThrew = false;
    try {
        p.revert();
    } catch (const std::system_error&) {
        revertThrew = true;
    }
    EXPECT_TRUE(revertThrew);

    EXPECT_CALL(*engine, execInNetNs("test-id", _)).WillOnce(Return(std::string{}));

    EXPECT_NO_THROW(p.revert());
}
