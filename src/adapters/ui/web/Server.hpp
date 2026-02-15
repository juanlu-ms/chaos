#pragma once

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <domain/ports/IContainerEngine.hpp>
#include <memory>

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
    explicit Server(std::shared_ptr<chaos::domain::ports::IContainerEngine> engine);
    ~Server() = default;

    /**
     * @brief Start listening on the given port.
     * @param port TCP port to bind on all interfaces.
     */
    void run(int port);

private:
    httplib::Server m_server;
    std::shared_ptr<chaos::domain::ports::IContainerEngine> m_engine;

    /**
     * @brief Register all HTTP routes.
     */
    void setupRoutes();
};

}  // namespace chaos::adapters::ui::web
