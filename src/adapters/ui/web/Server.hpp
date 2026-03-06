#pragma once

#include <httplib.h>

#include <domain/ports/IContainerEngine.hpp>

namespace chaos::adapters::ui::web {

/**
 * @brief HTTP server exposing the web API.
 */
class Server {
public:
    /**
     * @brief Construct the server with a container engine dependency.
     * @param engine Engine used to retrieve container data.
     */
    explicit Server(chaos::domain::ports::IContainerEngine& engine);
    ~Server() = default;

    /**
     * @brief Start listening on the given port.
     * @param port TCP port to bind on localhost (127.0.0.1).
     */
    void listen(int port);

private:
    httplib::Server m_server;
    chaos::domain::ports::IContainerEngine& m_engine;

    /**
     * @brief Register all HTTP routes.
     */
    void setupRoutes();
};

}  // namespace chaos::adapters::ui::web
