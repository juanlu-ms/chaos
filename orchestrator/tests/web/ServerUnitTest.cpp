/**
 * @file ServerUnitTest.cpp
 * @brief Unit tests for the HTTP/SSE web Server using ephemeral port and MockContainerEngine.
 */

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "MockContainerEngine.hpp"
#include "containers/SystemInfo.hpp"
#include "interfaces/web/Server.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::interfaces::web;
using namespace testing;
using json = nlohmann::json;

namespace {

class ServerTest : public ::testing::Test {
protected:
    std::shared_ptr<tests::MockContainerEngine> engine_;
    std::unique_ptr<Server> server_;
    std::jthread serverThread_;
    int port_ = 0;

    void SetUp() override {
        engine_ = std::make_shared<NiceMock<tests::MockContainerEngine>>();
        server_ = std::make_unique<Server>(engine_);
    }

    void TearDown() override {
        if (port_ > 0) {
            httplib::Client cli("127.0.0.1", port_);
            cli.set_connection_timeout(0, 100000);
            cli.Post("/api/run/abort");
        }
        if (server_) {
            server_->stop();
        }
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }

    void startServer() {
        for (int attempt = 0; attempt < 5; ++attempt) {
            static int offset = 0;
            port_ = 15000 + ((offset++) % 50000);
            serverThread_ = std::jthread([this]() {
                try {
                    server_->listen(port_);
                } catch (...) {
                }
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            httplib::Client cli("127.0.0.1", port_);
            cli.set_connection_timeout(0, 500000);
            auto res = cli.Get("/api/status");
            if (res && res->status == 200) {
                return;
            }
            if (serverThread_.joinable()) {
                server_->stop();
                serverThread_.join();
            }
        }
        FAIL() << "Could not bind server to an ephemeral port";
    }

    httplib::Client client() {
        httplib::Client cli("127.0.0.1", port_);
        cli.set_connection_timeout(0, 500000);
        return cli;
    }
};

}  // namespace

/**
 * @test Verifies GET /api/status returns 200 with expected JSON fields.
 */
TEST_F(ServerTest, StatusReturnsOk) {
    EXPECT_CALL(*engine_, listContainers()).WillRepeatedly(Return(std::vector<containers::Container>{}));
    EXPECT_CALL(*engine_, getSystemInfo()).WillRepeatedly(Return(containers::SystemInfo{8589934592}));
    startServer();

    auto res = client().Get("/api/status");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = json::parse(res->body);
    EXPECT_EQ(body["project"], "Chaos Engine");
    EXPECT_EQ(body["status"], "Online (Web Adapter)");
}

/**
 * @test Verifies GET /api/containers returns container list from engine.
 */
TEST_F(ServerTest, TargetsReturnsContainerList) {
    containers::Container ctr;
    ctr.id = "abc123";
    ctr.name = "test-ctr";
    ctr.state = "running";

    EXPECT_CALL(*engine_, listContainers()).WillRepeatedly(Return(std::vector<containers::Container>{ctr}));
    EXPECT_CALL(*engine_, getSystemInfo()).WillRepeatedly(Return(containers::SystemInfo{8589934592}));
    startServer();

    auto res = client().Get("/api/containers");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = json::parse(res->body);
    ASSERT_TRUE(body.is_array());
    ASSERT_EQ(body.size(), 1u);
    EXPECT_EQ(body[0]["id"], "abc123");
    EXPECT_EQ(body[0]["name"], "test-ctr");
    EXPECT_EQ(body[0]["state"], "running");
}

/**
 * @test Verifies GET /api/limits returns system info from engine.
 */
TEST_F(ServerTest, LimitsReturnsSystemInfo) {
    containers::SystemInfo info;
    info.memTotal = 8589934592;

    EXPECT_CALL(*engine_, listContainers()).WillRepeatedly(Return(std::vector<containers::Container>{}));
    EXPECT_CALL(*engine_, getSystemInfo()).WillRepeatedly(Return(info));
    startServer();

    auto res = client().Get("/api/limits");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = json::parse(res->body);
    EXPECT_TRUE(body.contains("memory_total_mb"));
    EXPECT_TRUE(body.contains("perturbation_limits"));
}

/**
 * @test Verifies POST /api/run with valid JSON manifest returns 202.
 */
TEST_F(ServerTest, RunWithValidManifestReturnsStarted) {
    EXPECT_CALL(*engine_, listContainers()).WillRepeatedly(Return(std::vector<containers::Container>{}));
    EXPECT_CALL(*engine_, getSystemInfo()).WillRepeatedly(Return(containers::SystemInfo{8589934592}));
    EXPECT_CALL(*engine_, getStatus(_)).WillRepeatedly(Return(containers::ContainerStatus::Running));
    EXPECT_CALL(*engine_, getLogs(_)).WillRepeatedly(Return(std::string{}));
    EXPECT_CALL(*engine_, getContainerIp(_)).WillRepeatedly(Return(std::string{"127.0.0.1"}));

    startServer();

    std::string manifest = R"({
        "test_name": "unit-test",
        "target": {"id": "test-ctr"},
        "perturbations": [],
        "expectations": [{"type": "container_running", "parameters": {}}]
    })";

    auto res = client().Post("/api/run", manifest, "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 202);

    auto body = json::parse(res->body);
    EXPECT_EQ(body["status"], "started");
}

/**
 * @test Verifies POST /api/run with empty body returns 400.
 */
TEST_F(ServerTest, RunWithEmptyBodyReturns400) {
    EXPECT_CALL(*engine_, listContainers()).WillRepeatedly(Return(std::vector<containers::Container>{}));
    EXPECT_CALL(*engine_, getSystemInfo()).WillRepeatedly(Return(containers::SystemInfo{8589934592}));
    startServer();

    auto res = client().Post("/api/run", "", "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);
}

/**
 * @test Verifies POST /api/run with invalid JSON returns 400.
 */
TEST_F(ServerTest, RunWithInvalidJsonReturns400) {
    EXPECT_CALL(*engine_, listContainers()).WillRepeatedly(Return(std::vector<containers::Container>{}));
    EXPECT_CALL(*engine_, getSystemInfo()).WillRepeatedly(Return(containers::SystemInfo{8589934592}));
    startServer();

    auto res = client().Post("/api/run", "not json", "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);
}

/**
 * @test Verifies GET /api/containers propagates engine errors as HTTP 500.
 */
TEST_F(ServerTest, TargetsReturns500OnEngineError) {
    EXPECT_CALL(*engine_, listContainers())
        .WillRepeatedly(Throw(containers::ContainerEngineError("connection refused")));
    EXPECT_CALL(*engine_, getSystemInfo()).WillRepeatedly(Return(containers::SystemInfo{8589934592}));
    startServer();

    auto res = client().Get("/api/containers");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 500);
}

/**
 * @test Verifies POST /api/run/abort returns 409 when no active run.
 */
TEST_F(ServerTest, AbortWithoutActiveRunReturns409) {
    EXPECT_CALL(*engine_, listContainers()).WillRepeatedly(Return(std::vector<containers::Container>{}));
    EXPECT_CALL(*engine_, getSystemInfo()).WillRepeatedly(Return(containers::SystemInfo{8589934592}));
    startServer();

    auto res = client().Post("/api/run/abort");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 409);
}
