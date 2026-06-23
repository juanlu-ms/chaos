/**
 * @file MemoryCapPerturbationUnitTest.cpp
 * @brief Unit tests for MemoryCapPerturbation apply/revert behavior.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "MockContainerEngine.hpp"
#include "perturbations/internal/MemoryCapPerturbation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::perturbations;
using namespace testing;

/**
 * @test Verifies revert does not clear the applied flag when the initial revert attempt fails.
 */
TEST(MemoryCapPerturbationTest, RevertDoesNotClearAppliedFlagOnFailure) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    manifests::Perturbation spec;
    spec.type = "memory_cap";
    spec.parameters["limit_bytes"] = "1048576";

    containers::SystemInfo sys_info;
    sys_info.mem_total = 4294967296;

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(sys_info));

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576)).Times(1);

    MemoryCapPerturbation p(engine, "test-id", spec);
    p.apply();

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", sys_info.mem_total))
        .WillOnce(Throw(containers::ContainerEngineError("update failed")));

    bool revert_threw = false;
    try {
        p.revert();
    } catch (const std::system_error&) {
        revert_threw = true;
    }
    EXPECT_TRUE(revert_threw);

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", sys_info.mem_total)).Times(1);

    EXPECT_NO_THROW(p.revert());
}

/**
 * @test Verifies apply stores the original memory limit for restoration on revert.
 */
TEST(MemoryCapPerturbationTest, StoresOriginalMemoryInApply) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    manifests::Perturbation spec;
    spec.type = "memory_cap";
    spec.parameters["limit_bytes"] = "1048576";

    containers::SystemInfo sys_info;
    sys_info.mem_total = 4294967296;

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(sys_info));

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576)).Times(1);

    MemoryCapPerturbation p(engine, "test-id", spec);
    p.apply();

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", sys_info.mem_total)).Times(1);

    p.revert();
}
