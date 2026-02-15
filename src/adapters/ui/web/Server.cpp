#include <adapters/ui/web/Server.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace chaos::adapters::ui::web {

Server::Server(std::shared_ptr<chaos::domain::ports::IContainerEngine> engine) : m_engine(std::move(engine)) {
    setupRoutes();
}

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

    m_server.Get("/containers", [this](const httplib::Request&, httplib::Response& res) {
        try {
            spdlog::debug("/containers requested");
            auto containers = m_engine->listContainers();
            json j = json::array();
            for (const auto& c : containers) {
                j.push_back({{"id", c.id}, {"name", c.name}, {"state", c.state}});
            }
            res.set_content(j.dump(4), "application/json");
            spdlog::info("/containers served: {} items", containers.size());
        } catch (const std::exception& ex) {
            json err;
            err["error"] = ex.what();
            res.status = 500;
            res.set_content(err.dump(4), "application/json");
            spdlog::error("/containers failed: {}", ex.what());
        }
    });
}

}  // namespace chaos::adapters::ui::web
