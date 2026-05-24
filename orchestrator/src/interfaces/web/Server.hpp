#pragma once

#include <httplib.h>

#include <memory>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <string_view>
#include <thread>

#include "containers/IContainerEngine.hpp"
#include "core/ChaosRunner.hpp"
#include "core/RunSession.hpp"

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
     */
    explicit Server(std::shared_ptr<containers::IContainerEngine> engine);
    ~Server() = default;

    /**
     * @brief Start listening on the given port (blocking call).
     * @param port TCP port number
     */
    void listen(int port);

private:
    httplib::Server server_;
    std::shared_ptr<containers::IContainerEngine> engine_;
    core::ChaosRunner runner_;

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

    void executeRunAsync(manifests::ChaosManifest manifest, std::shared_ptr<RunSession> session);
    void handleContainerAction(const httplib::Request& req, httplib::Response& res, std::string_view actionName);

    void writeSSEEvent(const httplib::DataSink& sink, const nlohmann::json& data);
};

}  // namespace chaos::orchestrator::interfaces::web
