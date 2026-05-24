#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "MockContainerEngine.hpp"
#include "perturbations/internal/MemoryCapPerturbation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::perturbations;
using namespace testing;

TEST(MemoryCapPerturbationTest, RevertDoesNotClearAppliedFlagOnFailure) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    manifests::Perturbation spec;
    spec.type = "memory_cap";
    spec.parameters["limit_bytes"] = "1048576";

    containers::SystemInfo sysInfo;
    sysInfo.memTotal = 4294967296;

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(sysInfo));

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576)).Times(1);

    MemoryCapPerturbation p(engine, "test-id", spec);
    p.apply();

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", sysInfo.memTotal))
        .WillOnce(Throw(containers::ContainerEngineError("update failed")));

    bool revertThrew = false;
    try {
        p.revert();
    } catch (const std::system_error&) {
        revertThrew = true;
    }
    EXPECT_TRUE(revertThrew);

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", sysInfo.memTotal)).Times(1);

    EXPECT_NO_THROW(p.revert());
}

TEST(MemoryCapPerturbationTest, StoresOriginalMemoryInApply) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    manifests::Perturbation spec;
    spec.type = "memory_cap";
    spec.parameters["limit_bytes"] = "1048576";

    containers::SystemInfo sysInfo;
    sysInfo.memTotal = 4294967296;

    EXPECT_CALL(*engine, getSystemInfo()).WillOnce(Return(sysInfo));

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", 1048576)).Times(1);

    MemoryCapPerturbation p(engine, "test-id", spec);
    p.apply();

    EXPECT_CALL(*engine, updateMemoryLimit("test-id", sysInfo.memTotal)).Times(1);

    p.revert();
}
