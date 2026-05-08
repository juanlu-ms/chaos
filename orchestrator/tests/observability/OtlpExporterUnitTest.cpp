#include <gtest/gtest.h>

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
