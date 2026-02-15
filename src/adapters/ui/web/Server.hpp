#pragma once

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

namespace chaos::adapters::ui::web {

class Server {
public:
    Server();
    ~Server() = default;

    void run(int port);

private:
    httplib::Server m_server;

    void setupRoutes();
};

}  // namespace chaos::adapters::ui::web
