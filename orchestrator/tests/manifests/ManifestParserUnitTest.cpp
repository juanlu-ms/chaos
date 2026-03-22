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

/**
 * @file ManifestParserUnitTest.cpp
 * @brief Unit tests for manifest parsing and validation rules.
 */

using chaos::orchestrator::manifests::ChaosManifest;
using chaos::orchestrator::manifests::ManifestParser;

namespace {

class TempManifestFileError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Fixture for manifest parser tests.
 */
class ManifestParserUnitTest : public ::testing::Test {
private:
    std::vector<std::filesystem::path> tempFiles;

protected:
    std::string createTempManifest(const std::string& json) {
        static std::atomic<unsigned long long> counter{0};
        const auto id = counter.fetch_add(1, std::memory_order_relaxed);
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto filename = fmt::format("chaos_manifest_parser_{}_{}.json", now, id);
        const auto path = std::filesystem::temp_directory_path() / filename;

        std::ofstream output(path);
        if (!output.is_open()) {
            throw TempManifestFileError("Cannot create temporary manifest file");
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
  "target": { "id": "orders-api" },
  "perturbations": [
    { "type": "kill", "parameters": {} }
  ],
  "expectations": [
    { "type": "container_not_running", "parameters": {} }
  ]
}
)json");

    const ChaosManifest manifest = ManifestParser::parse(file);

    EXPECT_EQ(manifest.test_name, "minimal");
    EXPECT_EQ(manifest.target.id, "orders-api");

    ASSERT_EQ(manifest.perturbations.size(), 1U);
    EXPECT_EQ(manifest.perturbations[0].type, "kill");
    EXPECT_TRUE(manifest.perturbations[0].parameters.empty());

    ASSERT_EQ(manifest.expectations.size(), 1U);
    EXPECT_EQ(manifest.expectations[0].type, "container_not_running");
    EXPECT_TRUE(manifest.expectations[0].parameters.empty());
}

/**
 * @test Verifies parsing of all supported perturbation and expectation types.
 */
TEST_F(ManifestParserUnitTest, ParsesAllSupportedPerturbationAndExpectationTypes) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "all-supported-types",
  "target": { "id": "payments-api" },
  "perturbations": [
    { "type": "kill", "parameters": {} },
    { "type": "memory_cap", "parameters": { "limit_bytes": "268435456" } },
    { "type": "cpu_cap", "parameters": { "quota": "25000", "period": "100000" } },
    { "type": "network_delay", "parameters": { "delay_ms": "120" } }
  ],
  "expectations": [
    { "type": "container_running", "parameters": {} },
    { "type": "container_not_running", "parameters": {} },
    { "type": "log_contains", "parameters": { "substring": "ready" } },
    { "type": "log_not_contains", "parameters": { "substring": "panic" } }
  ]
}
)json");

    const ChaosManifest manifest = ManifestParser::parse(file);

    ASSERT_EQ(manifest.perturbations.size(), 4U);
    EXPECT_EQ(manifest.perturbations[0].type, "kill");
    EXPECT_EQ(manifest.perturbations[1].type, "memory_cap");
    EXPECT_EQ(manifest.perturbations[1].parameters.at("limit_bytes"), "268435456");
    EXPECT_EQ(manifest.perturbations[2].type, "cpu_cap");
    EXPECT_EQ(manifest.perturbations[2].parameters.at("quota"), "25000");
    EXPECT_EQ(manifest.perturbations[2].parameters.at("period"), "100000");
    EXPECT_EQ(manifest.perturbations[3].type, "network_delay");
    EXPECT_EQ(manifest.perturbations[3].parameters.at("delay_ms"), "120");

    ASSERT_EQ(manifest.expectations.size(), 4U);
    EXPECT_EQ(manifest.expectations[0].type, "container_running");
    EXPECT_EQ(manifest.expectations[1].type, "container_not_running");
    EXPECT_EQ(manifest.expectations[2].type, "log_contains");
    EXPECT_EQ(manifest.expectations[2].parameters.at("substring"), "ready");
    EXPECT_EQ(manifest.expectations[3].type, "log_not_contains");
    EXPECT_EQ(manifest.expectations[3].parameters.at("substring"), "panic");
}

/**
 * @test Verifies an exception is thrown when the manifest file does not exist.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenManifestFileDoesNotExist) {
    EXPECT_THROW((void)ManifestParser::parse("/tmp/chaos_manifest_file_that_does_not_exist.json"),
                 chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies an exception is thrown when JSON content is malformed.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenJsonIsMalformed) {
    const auto file = createTempManifest(R"json(
{ "test_name": "bad-json", "target": { "type": "container", "name": "x" }
)json");

    EXPECT_THROW((void)ManifestParser::parse(file), chaos::orchestrator::manifests::ManifestParserError);
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

    EXPECT_NO_THROW((void)ManifestParser::parse(file));
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

    EXPECT_THROW((void)ManifestParser::parse(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies unsupported perturbation types are rejected.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenPerturbationTypeIsUnsupported) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "unknown-perturbation",
  "target": { "id": "orders-api" },
  "perturbations": [ { "type": "disk_fill", "parameters": {} } ],
  "expectations": [ { "type": "container_not_running", "parameters": {} } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parse(file), chaos::orchestrator::manifests::ManifestParserError);
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
  "expectations": [ { "type": "exit_code_equals", "parameters": { "code": "0" } } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parse(file), chaos::orchestrator::manifests::ManifestParserError);
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

    EXPECT_THROW((void)ManifestParser::parse(file), chaos::orchestrator::manifests::ManifestParserError);
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

    EXPECT_THROW((void)ManifestParser::parse(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies cpu_cap perturbation requires both quota and period parameters.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenCpuCapHasMissingPeriod) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "missing-cpu-period",
  "target": { "id": "orders-api" },
  "perturbations": [ { "type": "cpu_cap", "parameters": { "quota": "25000" } } ],
  "expectations": [ { "type": "container_running", "parameters": {} } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parse(file), chaos::orchestrator::manifests::ManifestParserError);
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

    EXPECT_THROW((void)ManifestParser::parse(file), chaos::orchestrator::manifests::ManifestParserError);
}

/**
 * @test Verifies unsupported network_cap is rejected after unifying network type to network_delay.
 */
TEST_F(ManifestParserUnitTest, ThrowsWhenLegacyNetworkCapTypeIsUsed) {
    const auto file = createTempManifest(R"json(
{
  "test_name": "legacy-network-cap",
  "target": { "id": "orders-api" },
  "perturbations": [ { "type": "network_cap", "parameters": { "latency_ms": "120" } } ],
  "expectations": [ { "type": "container_running", "parameters": {} } ]
}
)json");

    EXPECT_THROW((void)ManifestParser::parse(file), chaos::orchestrator::manifests::ManifestParserError);
}

}  // namespace
