#pragma once

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <domain/ports/IContainerEngine.hpp>
#include <memory>

namespace chaos::adapters::ui::web {

class Server {
public:
    explicit Server(std::shared_ptr<chaos::domain::ports::IContainerEngine> engine);
    ~Server() = default;

    void run(int port);

private:
    httplib::Server m_server;
    std::shared_ptr<chaos::domain::ports::IContainerEngine> m_engine;

    void setupRoutes();
};

}  // namespace chaos::adapters::ui::web
