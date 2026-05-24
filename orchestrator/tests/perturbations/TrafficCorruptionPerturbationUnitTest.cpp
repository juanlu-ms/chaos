#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "MockContainerEngine.hpp"
#include "perturbations/internal/TrafficCorruptionPerturbation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::perturbations;
using namespace testing;

TEST(TrafficCorruptionPerturbationTest, RevertDoesNotClearAppliedFlagOnFailure) {
    auto engine = std::make_shared<tests::MockContainerEngine>();

    manifests::Perturbation spec;
    spec.type = "traffic_corruption";
    spec.parameters["corrupt_pct"] = "50";

    EXPECT_CALL(*engine, execInNetNs("test-id", _)).WillOnce(Return(std::string{}));

    TrafficCorruptionPerturbation p(engine, "test-id", spec);
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
