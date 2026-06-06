/**
 * @file LogValidationUnitTest.cpp
 * @brief Unit tests for LogContainsValidation and LogNotContainsValidation.
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/TargetState.hpp"
#include "manifests/Manifest.hpp"
#include "validation/internal/LogValidation.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::validation;

namespace {

core::TargetState makeState(const std::vector<std::string>& logs) {
    core::TargetState s;
    s.container_id = "test-ctr";
    s.status = containers::ContainerStatus::Running;
    s.recent_logs = logs;
    return s;
}

}  // namespace

/**
 * @test Verifies LogContainsValidation passes when substring is present.
 */
TEST(LogValidationTest, ContainsPassesWhenSubstringPresent) {
    LogContainsValidation v;
    auto result = v.validate(makeState({"hello world\n"}), {"log_contains", {{"substring", "hello"}}});
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.expectationType, "log_contains");
}

/**
 * @test Verifies LogContainsValidation fails when substring is absent.
 */
TEST(LogValidationTest, ContainsFailsWhenSubstringAbsent) {
    LogContainsValidation v;
    auto result = v.validate(makeState({"hello world\n"}), {"log_contains", {{"substring", "missing"}}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies LogNotContainsValidation passes when substring is absent.
 */
TEST(LogValidationTest, NotContainsPassesWhenSubstringAbsent) {
    LogNotContainsValidation v;
    auto result = v.validate(makeState({"all good\n"}), {"log_not_contains", {{"substring", "panic"}}});
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.expectationType, "log_not_contains");
}

/**
 * @test Verifies LogNotContainsValidation fails when substring is present.
 */
TEST(LogValidationTest, NotContainsFailsWhenSubstringPresent) {
    LogNotContainsValidation v;
    auto result = v.validate(makeState({"panic: nil ptr\n"}), {"log_not_contains", {{"substring", "panic"}}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies log validators handle empty logs vector.
 */
TEST(LogValidationTest, ContainsFailsWithEmptyLogs) {
    LogContainsValidation v;
    auto result = v.validate(makeState({}), {"log_contains", {{"substring", "missing"}}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies log validators handle empty substring parameter.
 */
TEST(LogValidationTest, ContainsPassesWithEmptySubstring) {
    LogContainsValidation v;
    auto result = v.validate(makeState({"any log\n"}), {"log_contains", {{"substring", ""}}});
    EXPECT_TRUE(result.passed);
}

/**
 * @test Verifies NotContains fails with empty substring (empty string is trivially contained).
 */
TEST(LogValidationTest, NotContainsFailsWithEmptySubstring) {
    LogNotContainsValidation v;
    auto result = v.validate(makeState({"any log\n"}), {"log_not_contains", {{"substring", ""}}});
    EXPECT_FALSE(result.passed);
}

/**
 * @test Verifies log_contains searches across multiple log lines.
 */
TEST(LogValidationTest, ContainsSearchesAllLines) {
    LogContainsValidation v;
    auto result =
        v.validate(makeState({"first\n", "second\n", "third\n"}), {"log_contains", {{"substring", "second"}}});
    EXPECT_TRUE(result.passed);
}
