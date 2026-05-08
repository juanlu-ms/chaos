#pragma once

#include <httplib.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>

#include "containers/IContainerEngine.hpp"
#include "shared/TargetState.hpp"

namespace chaos::orchestrator::interfaces::web {

using json = nlohmann::json;

/**
 * Shared mutable state for a single chaos run, observable via SSE.
 */
struct RunSession {
    std::mutex mtx;
    std::condition_variable cv;
    std::optional<shared::TargetState> latest;
    bool running = false;
    bool complete = false;
    json results;
    std::string error;
    std::string phase;
};

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
    httplib::Server m_server;
    std::shared_ptr<containers::IContainerEngine> m_engine;

    std::mutex session_mutex_;
    std::shared_ptr<RunSession> m_session;

    void setupRoutes();
    void handleRun(const httplib::Request& req, httplib::Response& res);
    void handleEvents(const httplib::Request& req, httplib::Response& res);
    void handleTargets(const httplib::Request& req, httplib::Response& res);
    void handleLimits(const httplib::Request& req, httplib::Response& res);
    void handleAbort(const httplib::Request& req, httplib::Response& res);
};

}  // namespace chaos::orchestrator::interfaces::web
