#include <adapters/ui/web/Server.hpp>

using json = nlohmann::json;

namespace chaos::adapters::ui::web {

Server::Server() { setupRoutes(); }

void Server::run(int port) {
    spdlog::info("chaos listening to http://0.0.0.0:{}", port);
    m_server.listen("0.0.0.0", port);
}

void Server::setupRoutes() {
    m_server.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        json j;
        j["project"] = "chaos Engine";
        j["status"] = "Online (Web Adapter)";

        res.set_content(j.dump(4), "application/json");
    });
}

}  // namespace chaos::adapters::ui::web
