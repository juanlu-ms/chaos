/**
 * @file ManifestParserUnitTest.cpp
 * @brief Unit tests for manifest parsing and validation rules.
 */

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "manifests/ManifestParser.hpp"

using chaos::orchestrator::manifests::ChaosManifest;
using chaos::orchestrator::manifests::ManifestParser;

namespace UnitTest {

/**
 * @brief Fixture for manifest parser tests.
 */
class ManifestParserUnitTest : public ::testing::Test {
private:
    std::vector<std::filesystem::path> tempFiles;

protected:
    std::string createTempManifest(const std::string& json) {
        static std::atomic<unsigned long long> counter{0};
        const auto id = counter.fetch_add(1);
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto filename = fmt::format("chaos_manifest_parser_{}_{}.json", now, id);
        const auto path = std::filesystem::temp_directory_path() / filename;

        std::ofstream output(path);
        if (!output.is_open()) {
            throw std::runtime_error("Cannot create temporary manifest file");
        }
        output << json;
        output.close();

        tempFiles.push_back(path);
        return path.string();
    }

    void TearDown() override {
        for (const auto& path : tempFiles) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }
};

/**
 * @test Verifies parsing of a minimal valid manifest.
 */
TEST_F(ManifestParserUnitTest, ParsesMinimalValidManifest) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "minimal",
  "target": { "id": "1234" },
  "perturbations": [],
  "expectations": []
}
)json");

    const ChaosManifest manifest = ManifestParser::parseFromFile(file);

    EXPECT_EQ(manifest.test_name, "minimal");
    EXPECT_EQ(manifest.target.id, "1234");

    ASSERT_EQ(manifest.perturbations.size(), 0U);

    ASSERT_EQ(manifest.expectations.size(), 0U);
}

/**
 * @test Verifies parsing of all supported perturbation and expectation types.
 */
TEST_F(ManifestParserUnitTest, ParsesAllSupportedPerturbationAndExpectationTypes) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "all-supported-types",
  "target": { "id": "payments-api" },
  "duration_s": 30,
  "perturbations": [
    { "type": "kill", "parameters": {} },
    { "type": "memory_cap", "parameters": { "limit_bytes": "268435456" } },
    { "type": "cpu_cap", "parameters": { "cpu_cores": "2" } },
    { "type": "network_delay", "parameters": { "delay_ms": "120" } }
  ],
  "expectations": [
    { "type": "container_running", "parameters": {} },
    { "type": "container_not_running", "parameters": {} },
    { "type": "log_contains", "parameters": { "substring": "ready" } },
    { "type": "log_not_contains", "parameters": { "substring": "panic" } },
    { "type": "http_status", "parameters": { "port": "8080", "path": "/health", "expected_status": "200" } },
    { "type": "http_latency", "parameters": { "port": "8080", "path": "/process", "max_latency_ms": "500" } }
  ]
}
)json");

    const ChaosManifest manifest = ManifestParser::parseFromFile(file);

    ASSERT_EQ(manifest.duration_s, 30);

    ASSERT_EQ(manifest.perturbations.size(), 4U);
    EXPECT_EQ(manifest.perturbations[0].type, "kill");
    EXPECT_EQ(manifest.perturbations[1].type, "memory_cap");
    EXPECT_EQ(manifest.perturbations[1].parameters.at("limit_bytes"), "268435456");
    EXPECT_EQ(manifest.perturbations[2].type, "cpu_cap");
    EXPECT_EQ(manifest.perturbations[2].parameters.at("cpu_cores"), "2");
    EXPECT_EQ(manifest.perturbations[3].type, "network_delay");
    EXPECT_EQ(manifest.perturbations[3].parameters.at("delay_ms"), "120");

    ASSERT_EQ(manifest.expectations.size(), 6U);
    EXPECT_EQ(manifest.expectations[0].type, "container_running");
    EXPECT_EQ(manifest.expectations[1].type, "container_not_running");
    EXPECT_EQ(manifest.expectations[2].type, "log_contains");
    EXPECT_EQ(manifest.expectations[2].parameters.at("substring"), "ready");
    EXPECT_EQ(manifest.expectations[3].type, "log_not_contains");
    EXPECT_EQ(manifest.expectations[3].parameters.at("substring"), "panic");
    EXPECT_EQ(manifest.expectations[4].type, "http_status");
    EXPECT_EQ(manifest.expectations[4].parameters.at("port"), "8080");
    EXPECT_EQ(manifest.expectations[4].parameters.at("path"), "/health");
    EXPECT_EQ(manifest.expectations[4].parameters.at("expected_status"), "200");
    EXPECT_EQ(manifest.expectations[5].type, "http_latency");
    EXPECT_EQ(manifest.expectations[5].parameters.at("port"), "8080");
    EXPECT_EQ(manifest.expectations[5].parameters.at("path"), "/process");
    EXPECT_EQ(manifest.expectations[5].parameters.at("max_latency_ms"), "500");
}

/**
 * @test Verifies an exception is thrown when the manifest file does not exist.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenManifestFileDoesNotExist) {
    EXPECT_THROW((void)ManifestParser::parseFromFile("/tmp/chaos_manifest_file_that_does_not_exist.json"),
                 chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies an exception is thrown when JSON content is malformed.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenJsonIsMalformed) {
    const auto file = createTempManifest(R"json(
{ "test_name": "bad-json", "target": { "type": "container", "name": "x" }
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies unknown target metadata fields are ignored while target id is preserved.
 */
TEST_F(ManifestParserUnitTest, IgnoresAdditionalTargetFields) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "bad-target-type",
  "target": { "id": "orders-api", "type": "not_container" },
  "perturbations": [ { "type": "kill", "parameters": {} } ],
  "expectations": [ { "type": "container_not_running", "parameters": {} } ]
}
)json");

    const auto manifest = ManifestParser::parseFromFile(file);
    EXPECT_EQ(manifest.target.id, "orders-api");
    ASSERT_EQ(manifest.perturbations.size(), 1U);
    EXPECT_EQ(manifest.perturbations[0].type, "kill");
    ASSERT_EQ(manifest.expectations.size(), 1U);
    EXPECT_EQ(manifest.expectations[0].type, "container_not_running");
}

/**
 * @test Verifies an exception is thrown when target id is missing.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenTargetIdIsMissing) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "missing-target-id",
  "target": {},
  "perturbations": [ { "type": "kill", "parameters": {} } ],
  "expectations": [ { "type": "container_not_running", "parameters": {} } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies unsupported perturbation types are rejected.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenPerturbationTypeIsUnsupported) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "unknown-perturbation",
  "target": { "id": "orders-api" },
  "perturbations": [ { "type": "unknown", "parameters": {} } ],
  "expectations": [ { "type": "container_not_running", "parameters": {} } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies unsupported expectation types are rejected.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenExpectationTypeIsUnsupported) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "unknown-expectation",
  "target": { "id": "orders-api" },
  "perturbations": [ { "type": "kill", "parameters": {} } ],
  "expectations": [ { "type": "unknown", "parameters": { "code": "0" } } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies log expectations require the substring parameter.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenLogExpectationHasNoSubstring) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "missing-log-substring",
  "target": { "id": "orders-api" },
  "perturbations": [ { "type": "kill", "parameters": {} } ],
  "expectations": [ { "type": "log_contains", "parameters": {} } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies memory_cap perturbation requires limit_bytes parameter.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenMemoryCapHasNoLimitBytes) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "missing-memory-limit",
  "target": { "id": "orders-api" },
  "perturbations": [ { "type": "memory_cap", "parameters": {} } ],
  "expectations": [ { "type": "container_running", "parameters": {} } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies cpu_cap perturbation requires both quota and period parameters.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenCpuCapHasMissingCpuCores) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "missing-cpu-period",
  "target": { "id": "orders-api" },
  "perturbations": [ { "type": "cpu_cap", "parameters": {} } ],
  "expectations": [ { "type": "container_running", "parameters": {} } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies network_delay perturbation requires delay_ms parameter.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenNetworkDelayHasNoDelayMs) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "missing-network-delay",
  "target": { "id": "orders-api" },
  "perturbations": [ { "type": "network_delay", "parameters": {} } ],
  "expectations": [ { "type": "container_running", "parameters": {} } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies that a target can be specified via "name" field instead of "id".
 */
TEST_F(ManifestParserUnitTest, TargetCanBeSpecifiedByName) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "by-name",
  "target": { "name": "my-container" },
  "perturbations": [ { "type": "kill" } ],
  "expectations": []
}
)json");

    const auto manifest = ManifestParser::parseFromFile(file);
    EXPECT_EQ(manifest.target.id, "my-container");
}

/**
 * @test Verifies that a target missing both "id" and "name" throws a ManifestParserError.
 */
TEST_F(ManifestParserUnitTest, TargetMissingBothIdAndNameThrows) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "no-target-key",
  "target": { "something_else": "value" },
  "perturbations": [ { "type": "kill" } ],
  "expectations": []
}
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies that duration_s is parsed successfully.
 * TODO: Once duration_s is moved to a perturbation parameter, this test should be
 * updated to verify correct parsing from the parameters map instead of the top-level manifest field.
 */
TEST_F(ManifestParserUnitTest, DurationSIsParsed) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "duration-test",
  "target": { "id": "my-container" },
  "perturbations": [],
  "expectations": [],
  "duration_s": 15
}
)json");

    const auto manifest = ManifestParser::parseFromFile(file);
    EXPECT_TRUE(manifest.duration_s.has_value());
    EXPECT_EQ(manifest.duration_s.value(), 15u);
}

/**
 * @test Verifies that an exception is thrown when duration_s is not an integer.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenDurationSIsNotAnInteger) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "duration-not-integer",
  "target": { "id": "my-container" },
  "perturbations": [],
  "expectations": [],
  "duration_s": "not-an-integer"
}
)json");

    EXPECT_THROW((void)ManifestParser::parseFromFile(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies that duration_s absence does not throw.
 */
TEST_F(ManifestParserUnitTest, DurationOmissionParses) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "no-duration-test",
  "target": { "id": "my-container" },
  "perturbations": []
}
)json");

    const auto manifest = ManifestParser::parseFromFile(file);
    EXPECT_FALSE(manifest.duration_s.has_value());
}

/**
 * @test Verifies http_latency with "continuous": true is parsed correctly.
 */
TEST_F(ManifestParserUnitTest, HttpLatencyContinuousTrueIsParsed) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "http-latency-continuous",
  "target": { "id": "test-container" },
  "perturbations": [],
  "expectations": [
    { "type": "http_latency", "continuous": true,
      "parameters": { "port": "8080", "path": "/ping", "max_latency_ms": "500" } }
  ]
}
)json");

    const auto manifest = ManifestParser::parseFromFile(file);
    ASSERT_EQ(manifest.expectations.size(), 1u);
    EXPECT_EQ(manifest.expectations[0].type, "http_latency");
    EXPECT_TRUE(manifest.expectations[0].continuous);
}

/**
 * @test Verifies http_latency without "continuous" defaults to true
 *        (is in kContinuousTypes).
 */
TEST_F(ManifestParserUnitTest, HttpLatencyWithoutContinuousDefaultsToTrue) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "http-latency-no-continuous",
  "target": { "id": "test-container" },
  "perturbations": [],
  "expectations": [
    { "type": "http_latency",
      "parameters": { "port": "8080", "path": "/ping", "max_latency_ms": "500" } }
  ]
}
)json");

    const auto manifest = ManifestParser::parseFromFile(file);
    ASSERT_EQ(manifest.expectations.size(), 1u);
    EXPECT_EQ(manifest.expectations[0].type, "http_latency");
    EXPECT_TRUE(manifest.expectations[0].continuous);
}

/**
 * @test Verifies http_latency with "continuous": false overrides the default.
 */
TEST_F(ManifestParserUnitTest, HttpLatencyExplicitContinuousFalseOverridesDefault) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "http-latency-continuous-false",
  "target": { "id": "test-container" },
  "perturbations": [],
  "expectations": [
    { "type": "http_latency", "continuous": false,
      "parameters": { "port": "8080", "path": "/ping", "max_latency_ms": "500" } }
  ]
}
)json");

    const auto manifest = ManifestParser::parseFromFile(file);
    ASSERT_EQ(manifest.expectations.size(), 1u);
    EXPECT_EQ(manifest.expectations[0].type, "http_latency");
    EXPECT_FALSE(manifest.expectations[0].continuous);
}

}  // namespace UnitTest
