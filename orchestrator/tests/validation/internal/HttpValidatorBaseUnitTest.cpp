/**
 * @file HttpValidatorBaseUnitTest.cpp
 * @brief Unit tests for the shared HTTP validation base function.
 */

#include <gtest/gtest.h>

#include "validation/internal/HttpValidatorBase.hpp"

using namespace chaos::orchestrator::validation::detail;
using namespace chaos::orchestrator::validation;
using chaos::orchestrator::manifests::Expectation;
using chaos::orchestrator::shared::TargetState;

/**
 * @test performHttpValidation returns a result with the correct expectationType.
 */
TEST(HttpValidatorBaseTest, ReturnsResultWithExpectationType) {
    TargetState state;
    Expectation expectation;
    expectation.type = "http_status";
    expectation.parameters["url"] = "http://127.0.0.1:1/";
    expectation.parameters["expected_status"] = "200";
    expectation.parameters["timeout_ms"] = "100";

    auto result = performHttpValidation(state, expectation, [](int, double, ValidationResult& r) {
        r.passed = false;
        r.message = "never reached";
    });

    EXPECT_EQ(result.expectationType, "http_status");
    EXPECT_FALSE(result.passed);
}

/**
 * @test performHttpValidation returns failed when connection is refused.
 */
TEST(HttpValidatorBaseTest, ConnectionRefusedReturnsFailed) {
    TargetState state;
    Expectation expectation;
    expectation.type = "http_status";
    expectation.parameters["url"] = "http://127.0.0.1:1/";
    expectation.parameters["expected_status"] = "200";
    expectation.parameters["timeout_ms"] = "100";

    auto result = performHttpValidation(state, expectation, [](int, double, ValidationResult& r) {
        r.passed = true;  // Shouldn't be called if connection fails
        r.message = "unexpected";
    });

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.message.empty());
}
