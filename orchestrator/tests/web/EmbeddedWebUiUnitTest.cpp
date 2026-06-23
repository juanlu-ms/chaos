/**
 * @file EmbeddedWebUiUnitTest.cpp
 * @brief Tests for the embedded Web UI routes (served from binary via CMakeRC).
 *
 * Skipped when CHAOS_HAVE_EMBEDDED_WEB_UI is not defined (e.g. sanitizer CI jobs
 * where pnpm is not available and embedding is auto-disabled).
 */

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "MockContainerEngine.hpp"
#include "containers/SystemInfo.hpp"
#include "interfaces/web/Server.hpp"

using namespace chaos::orchestrator;
using namespace chaos::orchestrator::interfaces::web;
using namespace testing;

namespace {

class EmbeddedWebUiTest : public ::testing::Test {
protected:
    std::shared_ptr<tests::MockContainerEngine> engine_;
    std::unique_ptr<Server> server_;
    std::jthread serverThread_;
    int port_ = 0;

    void SetUp() override {
        engine_ = std::make_shared<NiceMock<tests::MockContainerEngine>>();
        EXPECT_CALL(*engine_, listContainers()).WillRepeatedly(Return(std::vector<containers::Container>{}));
        EXPECT_CALL(*engine_, getSystemInfo()).WillRepeatedly(Return(containers::SystemInfo{8589934592}));
        server_ = std::make_unique<Server>(engine_);
    }

    void TearDown() override {
        if (server_) server_->stop();
        if (serverThread_.joinable()) serverThread_.join();
    }

    void startServer() {
        for (int attempt = 0; attempt < 5; ++attempt) {
            static int offset = 0;
            port_ = 25000 + ((offset++) % 30000);
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
            if (res && res->status == 200) return;
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

#ifdef CHAOS_HAVE_EMBEDDED_WEB_UI

TEST_F(EmbeddedWebUiTest, RootReturnsIndexHtml) {
    startServer();

    auto res = client().Get("/");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("<div id=\"root\">"), std::string::npos);
}

TEST_F(EmbeddedWebUiTest, IndexHtmlReturns200) {
    startServer();

    auto res = client().Get("/index.html");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("<html"), std::string::npos);
}

TEST_F(EmbeddedWebUiTest, UnknownAssetReturns404) {
    startServer();

    auto res = client().Get("/assets/nonexistent-file.js");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
}

TEST_F(EmbeddedWebUiTest, ApiStatusStillWorks) {
    startServer();

    auto res = client().Get("/api/status");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
}

#else

TEST_F(EmbeddedWebUiTest, SkippedNoEmbeddedUi) {
    GTEST_SKIP() << "CHAOS_HAVE_EMBEDDED_WEB_UI not defined — skipping embedded UI tests";
}

#endif
