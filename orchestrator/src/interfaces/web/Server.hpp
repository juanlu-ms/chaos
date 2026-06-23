#pragma once

#include <httplib.h>

#include <memory>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string_view>
#include <thread>

#include "containers/IContainerEngine.hpp"
#include "core/ChaosRunner.hpp"
#include "core/RunSession.hpp"
#include "history/IRunHistory.hpp"

#ifdef CHAOS_HAVE_EMBEDDED_WEB_UI
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(chaos_web);
#endif

namespace chaos::orchestrator::interfaces::web {

using core::RunSession;

/**
 * HTTP server exposing a REST API and Server-Sent Events for chaos engineering runs.
 */
class Server {
public:
    /**
     * @brief Construct a Server backed by the given container engine.
     * @param engine shared container engine implementation
     * @param history Optional run history for storing and querying past runs.
     * @param otlpEndpoint Optional OTLP HTTP endpoint for exporting run events.
     */
    explicit Server(std::shared_ptr<containers::IContainerEngine> engine,
                    std::shared_ptr<history::IRunHistory> history = nullptr,
                    std::optional<std::string> otlpEndpoint = std::nullopt);
    ~Server();

    /**
     * @brief Start listening on the given port (blocking call).
     * @param port TCP port number
     */
    void listen(int port);

    /**
     * @brief Stop the underlying HTTP server.
     *
     * Causes listen() to return, allowing graceful shutdown.
     */
    void stop();

private:
    httplib::Server server_;
    std::shared_ptr<containers::IContainerEngine> engine_;
    std::shared_ptr<history::IRunHistory> history_;
    core::ChaosRunner runner_;
    std::optional<std::string> otlpEndpoint_;

#ifdef CHAOS_HAVE_EMBEDDED_WEB_UI
    std::optional<cmrc::embedded_filesystem> webFs_;
    void registerEmbeddedRoutes();
    void serveEmbedded(const httplib::Request& req, httplib::Response& res, const std::string& path) const;
#endif

    mutable std::mutex session_mutex_;
    std::shared_ptr<RunSession> session_;
    std::jthread run_thread_;

    void setupRoutes();
    void setupApiRoutes();

    void handleRun(const httplib::Request& req, httplib::Response& res);
    void handleEvents(const httplib::Request& req, httplib::Response& res);
    void handleTargets(const httplib::Request& req, httplib::Response& res);
    void handleLimits(const httplib::Request& req, httplib::Response& res);
    void handleAbort(const httplib::Request& req, httplib::Response& res);
    void handleHistoryList(const httplib::Request& req, httplib::Response& res);
    void handleHistoryGet(const httplib::Request& req, httplib::Response& res);
    void handleHistoryDelete(const httplib::Request& req, httplib::Response& res);
    void handleHistoryClear(const httplib::Request& req, httplib::Response& res);

    void executeRunAsync(manifests::ChaosManifest manifest, std::shared_ptr<RunSession> session);
    void handleContainerAction(const httplib::Request& req, httplib::Response& res, std::string_view actionName);

    void writeSSEEvent(const httplib::DataSink& sink, const nlohmann::json& data);
};

}  // namespace chaos::orchestrator::interfaces::web
