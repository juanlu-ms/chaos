/**
 * @file CpuCapPerturbationUnitTest.cpp
 * @brief Unit tests for CpuCapPerturbation apply/revert edge cases.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "perturbations/internal/CpuCapPerturbation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::perturbations;
using namespace testing;

/**
 * @test Verifies revert does not clear the applied flag when the revert call fails.
 */
TEST(CpuCapPerturbationTest, RevertDoesNotClearAppliedFlagOnFailure) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    manifests::Perturbation spec;
    spec.type = "cpu_cap";
    spec.parameters["cpu_cores"] = "2";

    EXPECT_CALL(*engine, updateCpuQuota(std::string_view("test-id"), 200000, 100000)).Times(1);

    CpuCapPerturbation p(engine, "test-id", spec);
    p.apply();

    EXPECT_CALL(*engine, updateCpuQuota(std::string_view("test-id"), -1, 100000))
        .WillOnce(Throw(containers::ContainerEngineError("update failed")));

    bool revertThrew = false;
    try {
        p.revert();
    } catch (const std::system_error&) {
        revertThrew = true;
    }
    EXPECT_TRUE(revertThrew);

    EXPECT_CALL(*engine, updateCpuQuota(std::string_view("test-id"), -1, 100000)).Times(1);

    EXPECT_NO_THROW(p.revert());
}

/**
 * @test Verifies apply stores the reset values so revert restores to unlimited.
 */
TEST(CpuCapPerturbationTest, StoresOriginalQuotaInApply) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    manifests::Perturbation spec;
    spec.type = "cpu_cap";
    spec.parameters["cpu_cores"] = "2";

    EXPECT_CALL(*engine, updateCpuQuota(std::string_view("test-id"), 200000, 100000)).Times(1);

    CpuCapPerturbation p(engine, "test-id", spec);
    p.apply();

    EXPECT_CALL(*engine, updateCpuQuota(std::string_view("test-id"), -1, 100000)).Times(1);

    p.revert();
}

/**
 * @test Verifies revert is a no-op when apply has not been called.
 */
TEST(CpuCapPerturbationTest, RevertIsNoOpBeforeApply) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    manifests::Perturbation spec;
    spec.type = "cpu_cap";
    spec.parameters["cpu_cores"] = "1";

    EXPECT_CALL(*engine, updateCpuQuota(_, _, _)).Times(0);

    CpuCapPerturbation p(engine, "test-id", spec);
    EXPECT_NO_THROW(p.revert());
}
