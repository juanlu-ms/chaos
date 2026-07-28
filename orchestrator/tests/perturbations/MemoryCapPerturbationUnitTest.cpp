/**
 * @file MemoryCapPerturbationUnitTest.cpp
 * @brief Unit tests for MemoryCapPerturbation apply/revert behavior.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <system_error>

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

namespace {

/** @brief Builds the spec shared by the update-failure tests below. */
manifests::Perturbation makeSpec() {
    manifests::Perturbation spec;
    spec.type = "memory_cap";
    spec.parameters["limit_bytes"] = "1048576";
    return spec;
}

/** @brief System info whose mem_total is the value revert must restore. */
containers::SystemInfo makeSystemInfo() {
    containers::SystemInfo sys_info;
    sys_info.mem_total = 4294967296;
    return sys_info;
}

}  // namespace

/**
 * @test Verifies a cap that OOM-kills the target counts as applied even when the update reports an
 *       error, and that the target is left revertible.
 */
TEST(MemoryCapPerturbationTest, TreatsUpdateFailureAsAppliedWhenOomKilled) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    const auto sys_info = makeSystemInfo();

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(sys_info));
    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576))
        .WillOnce(Throw(containers::ContainerEngineApiError("HTTP 500: runc did not terminate successfully")));
    EXPECT_CALL(*engine, wasOomKilled("test-id")).WillOnce(Return(true));

    MemoryCapPerturbation p(engine, "test-id", makeSpec());
    EXPECT_NO_THROW(p.apply());

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", sys_info.mem_total)).Times(1);
    EXPECT_NO_THROW(p.revert());
}

/**
 * @test Verifies a target that died without OOM evidence still counts as applied.
 */
TEST(MemoryCapPerturbationTest, TreatsUpdateFailureAsAppliedWhenContainerDiedWithoutOomEvidence) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    const auto sys_info = makeSystemInfo();

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(sys_info));
    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576))
        .WillOnce(Throw(containers::ContainerEngineApiError("HTTP 500")));
    EXPECT_CALL(*engine, wasOomKilled("test-id")).WillOnce(Return(false));
    EXPECT_CALL(*engine, getStatus("test-id")).WillOnce(Return(containers::ContainerStatus::Exited));

    MemoryCapPerturbation p(engine, "test-id", makeSpec());
    EXPECT_NO_THROW(p.apply());

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", sys_info.mem_total)).Times(1);
    EXPECT_NO_THROW(p.revert());
}

/**
 * @test Verifies a target reported as Dead is treated like an exited one.
 */
TEST(MemoryCapPerturbationTest, TreatsUpdateFailureAsAppliedWhenContainerIsDead) {
    auto engine = std::make_shared<tests::MockContainerEngine>();
    const auto sys_info = makeSystemInfo();

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(sys_info));
    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576))
        .WillOnce(Throw(containers::ContainerEngineApiError("HTTP 500")));
    EXPECT_CALL(*engine, wasOomKilled("test-id")).WillOnce(Return(false));
    EXPECT_CALL(*engine, getStatus("test-id")).WillOnce(Return(containers::ContainerStatus::Dead));

    MemoryCapPerturbation p(engine, "test-id", makeSpec());
    EXPECT_NO_THROW(p.apply());

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", sys_info.mem_total)).Times(1);
    EXPECT_NO_THROW(p.revert());
}

/**
 * @test Verifies the error is rethrown when the target survives the failed update, and that the
 *       state is re-checked the configured number of times before giving up.
 */
TEST(MemoryCapPerturbationTest, RethrowsWhenContainerSurvivesUpdateFailure) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(makeSystemInfo()));
    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576))
        .WillOnce(Throw(containers::ContainerEngineApiError("HTTP 500")));
    EXPECT_CALL(*engine, wasOomKilled("test-id")).Times(3).WillRepeatedly(Return(false));
    EXPECT_CALL(*engine, getStatus("test-id")).Times(3).WillRepeatedly(Return(containers::ContainerStatus::Running));

    MemoryCapPerturbation p(engine, "test-id", makeSpec());
    EXPECT_THROW(p.apply(), std::system_error);

    // apply() failed, so the perturbation was never applied and revert must not touch the engine.
    EXPECT_CALL(*engine, updateMemoryLimit("test-id", _)).Times(0);
    EXPECT_NO_THROW(p.revert());
}

/**
 * @test Verifies the retry catches a target whose death is reported only on a later check.
 */
TEST(MemoryCapPerturbationTest, TreatsUpdateFailureAsAppliedWhenContainerDiesLate) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(makeSystemInfo()));
    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576))
        .WillOnce(Throw(containers::ContainerEngineApiError("HTTP 500")));
    EXPECT_CALL(*engine, wasOomKilled("test-id")).WillRepeatedly(Return(false));
    EXPECT_CALL(*engine, getStatus("test-id"))
        .WillOnce(Return(containers::ContainerStatus::Running))
        .WillOnce(Return(containers::ContainerStatus::Exited));

    MemoryCapPerturbation p(engine, "test-id", makeSpec());
    EXPECT_NO_THROW(p.apply());
}

/**
 * @test Verifies an inconclusive state is not treated as a successful application.
 */
TEST(MemoryCapPerturbationTest, RethrowsWhenStatusIsInconclusive) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(makeSystemInfo()));
    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576))
        .WillOnce(Throw(containers::ContainerEngineApiError("HTTP 500")));
    EXPECT_CALL(*engine, wasOomKilled("test-id")).WillOnce(Return(false));
    EXPECT_CALL(*engine, getStatus("test-id")).WillOnce(Return(containers::ContainerStatus::Unknown));

    MemoryCapPerturbation p(engine, "test-id", makeSpec());
    EXPECT_THROW(p.apply(), std::system_error);
}

/**
 * @test Verifies a failing state lookup surfaces the update error, not the lookup error.
 */
TEST(MemoryCapPerturbationTest, RethrowsOriginalErrorWhenStatusLookupFails) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(makeSystemInfo()));
    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576))
        .WillOnce(Throw(containers::ContainerEngineApiError("original update failure")));
    EXPECT_CALL(*engine, wasOomKilled("test-id"))
        .WillOnce(Throw(containers::ContainerEngineApiError("status lookup failure")));

    MemoryCapPerturbation p(engine, "test-id", makeSpec());
    try {
        p.apply();
        FAIL() << "apply() was expected to throw";
    } catch (const std::system_error& e) {
        EXPECT_THAT(e.what(), HasSubstr("original update failure"));
        EXPECT_THAT(e.what(), Not(HasSubstr("status lookup failure")));
    }
}

/**
 * @test Verifies the tolerance covers the update only: a failure to read system info means no cap
 *       was ever sent, so a dead target must not make it count as applied.
 */
TEST(MemoryCapPerturbationTest, RethrowsWhenSystemInfoFails) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Throw(containers::ContainerEngineApiError("info failure")));
    EXPECT_CALL(*engine, wasOomKilled(_)).Times(0);
    EXPECT_CALL(*engine, getStatus(_)).Times(0);
    EXPECT_CALL(*engine, updateMemoryLimit(_, _)).Times(0);

    MemoryCapPerturbation p(engine, "test-id", makeSpec());
    EXPECT_THROW(p.apply(), std::system_error);
    EXPECT_NO_THROW(p.revert());
}
