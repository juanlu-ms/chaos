/**
 * @file ValidationFactoryUnitTest.cpp
 * @brief Unit tests for the createValidator factory free function.
 */

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "manifests/Manifest.hpp"
#include "validation/IValidation.hpp"
#include "validation/ValidationFactory.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::validation;

/**
 * @test Verifies createValidator returns a non-null validator for each known expectation type.
 */
TEST(ValidationFactoryTest, CreatesValidatorForEachExpectedType) {
    EXPECT_NE(createValidator({"container_running", {}}), nullptr);
    EXPECT_NE(createValidator({"container_not_running", {}}), nullptr);
    EXPECT_NE(createValidator({"log_contains", {{"substring", "test"}}}), nullptr);
    EXPECT_NE(createValidator({"log_not_contains", {{"substring", "test"}}}), nullptr);
    EXPECT_NE(createValidator({"http_status", {{"port", "8080"}, {"path", "/"}, {"expected_status", "200"}}}), nullptr);
    EXPECT_NE(createValidator({"http_latency", {{"port", "8080"}, {"path", "/"}, {"max_latency_ms", "100"}}}), nullptr);
}

/**
 * @test Verifies createValidator throws std::invalid_argument for unknown expectation types.
 */
TEST(ValidationFactoryTest, ThrowsOnUnknownType) {
    EXPECT_THROW(static_cast<void>(createValidator({"unknown_type", {}})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(createValidator({"", {}})), std::invalid_argument);
}
