#include <gtest/gtest.h>
#include <httplib.h>

#include <nlohmann/json.hpp>

#include "observability/OtlpExporter.hpp"

using namespace chaos::orchestrator::observability;

TEST(OtlpExporterTest, BuildLogPayload) {
    auto payload = OtlpExporter::buildLogPayload(
        "chaos-run-123", "run_started", R"({"test_name":"demo","target":"abc"})", {{"service.name", "chaos-engine"}});

    EXPECT_TRUE(payload.contains("resourceLogs"));
    auto& logs = payload["resourceLogs"];
    ASSERT_TRUE(logs.is_array());
    ASSERT_GE(logs.size(), 1);

    auto& resource = logs[0]["resource"];
    auto& attrs = resource["attributes"];
    bool found = false;
    for (const auto& a : attrs) {
        if (a["key"] == "service.name" && a["value"]["stringValue"] == "chaos-engine") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(OtlpExporterTest, BuildStateChangeEvent) {
    auto payload = OtlpExporter::buildLogPayload(
        "run-456", "state_changed", R"({"container_id":"abc","cpu_percent":45.2})", {{"service.name", "chaos-engine"}});

    auto& scopeLogs = payload["resourceLogs"][0]["scopeLogs"];
    ASSERT_TRUE(scopeLogs.is_array());
    ASSERT_GE(scopeLogs.size(), 1);

    auto& record = scopeLogs[0]["logRecords"][0];
    EXPECT_EQ(record["severityText"], "state_changed");
    EXPECT_TRUE(record["body"]["stringValue"].get<std::string>().find("cpu_percent") != std::string::npos);
}

TEST(OtlpExporterTest, ExportToEndpoint) {
    OtlpExporter exporter("http://localhost:4318");
    EXPECT_EQ(exporter.endpoint(), "http://localhost:4318/v1/logs");
}

TEST(OtlpExporterTest, EmptyAttributes) {
    auto payload = OtlpExporter::buildLogPayload("run-empty", "run_started", "", {});
    EXPECT_TRUE(payload.contains("resourceLogs"));
    auto& attrs = payload["resourceLogs"][0]["resource"]["attributes"];
    EXPECT_TRUE(attrs.empty() || attrs.is_null());
}

TEST(OtlpExporterTest, SpecialCharactersInBody) {
    std::string special = R"({"msg":"hello\nworld™ ✓"})";
    auto payload = OtlpExporter::buildLogPayload("run-special", "data", special, {{"key", "val-with-äöü"}});
    auto body = payload["resourceLogs"][0]["scopeLogs"][0]["logRecords"][0]["body"]["stringValue"].get<std::string>();
    EXPECT_EQ(body, special);
}

TEST(OtlpExporterTest, BuildLogPayloadHasTimestamp) {
    auto payload = OtlpExporter::buildLogPayload("run-ts", "test", "{}", {});
    auto& record = payload["resourceLogs"][0]["scopeLogs"][0]["logRecords"][0];
    EXPECT_TRUE(record.contains("timeUnixNano"));
    EXPECT_FALSE(record["timeUnixNano"].get<std::string>().empty());
}

TEST(OtlpExporterTest, BuildLogPayloadHasRunId) {
    auto payload = OtlpExporter::buildLogPayload("my-run-id", "test", "{}", {});
    auto& attrs = payload["resourceLogs"][0]["scopeLogs"][0]["logRecords"][0]["attributes"];
    bool found = false;
    for (const auto& a : attrs) {
        if (a["key"] == "run_id" && a["value"]["stringValue"] == "my-run-id") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(OtlpExporterTest, ExportReturnsFalseForUnreachableEndpoint) {
    OtlpExporter exporter("http://127.0.0.1:1");
    auto payload = OtlpExporter::buildLogPayload("x", "test", "{}", {});
    bool result = exporter.exportLogs(payload);
    EXPECT_FALSE(result);
}
