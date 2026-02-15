#include <adapters/ui/web/Server.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace chaos::adapters::ui::web {

/**
 * @brief Construct the server and register routes.
 * @param engine Engine used to retrieve container data.
 */
Server::Server(std::shared_ptr<chaos::domain::ports::IContainerEngine> engine) : m_engine(std::move(engine)) {
    setupRoutes();
}

/**
 * @brief Start the server from argc/argv, parsing --port.
 * @param argc Argument count.
 * @param argv Argument values.
 * @return Exit code (0 on success, non-zero on error).
 */
int Server::run(int argc, char* argv[]) {
    int port = 8080;
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--port") {
            try {
                port = std::stoi(argv[i + 1]);
            } catch (...) {
                spdlog::error("Invalid port number: {}", argv[i + 1]);
                return 1;
            }
        }
    }
    listen(port);
    return 0;
}

/**
 * @brief Start the HTTP server on a specific port.
 * @param port TCP port to bind on all interfaces.
 */
void Server::listen(int port) {
    spdlog::info("chaos listening to http://0.0.0.0:{}", port);
    m_server.listen("0.0.0.0", port);
}

/**
 * @brief Register HTTP routes for the web adapter.
 */
void Server::setupRoutes() {
    m_server.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        json j;
        j["project"] = "Chaos Engine";
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
