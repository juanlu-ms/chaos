/**
 * @file Server.hpp
 * @brief HTTP server exposing the web API.
 */

#pragma once

#include <httplib.h>

#include <containers/IContainerEngine.hpp>

namespace chaos::orchestrator::interfaces::web {

/**
 * @brief HTTP server exposing the web API.
 */
class Server {
public:
    /**
     * @brief Construct the server with a container engine dependency.
     * @param engine Engine used to retrieve container data.
     */
    explicit Server(std::shared_ptr<containers::IContainerEngine> engine);
    ~Server() = default;

    /**
     * @brief Start listening on the given port.
     * @param port TCP port to bind on localhost (127.0.0.1).
     */
    void listen(int port);

private:
    /** @brief The underlying HTTP server. */
    httplib::Server m_server;
    /** @brief The container engine instance. */
    std::shared_ptr<containers::IContainerEngine> m_engine;

    /**
     * @brief Register all HTTP routes.
     */
    void setupRoutes();
};

}  // namespace chaos::orchestrator::interfaces::web
