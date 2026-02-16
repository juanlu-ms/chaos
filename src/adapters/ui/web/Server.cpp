#include <adapters/ui/web/Server.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace chaos::adapters::ui::web {

namespace {
std::optional<std::filesystem::path> findWebRoot() {
    std::vector<std::filesystem::path> candidates = {
        "src/adapters/ui/web/static",
        "adapters/ui/web/static",
        "../src/adapters/ui/web/static",
        "../../src/adapters/ui/web/static",
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}
}  // namespace

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
    if (auto web_root = findWebRoot(); web_root.has_value()) {
        if (!m_server.set_mount_point("/", web_root->string())) {
            spdlog::warn("Failed to mount static UI from {}", web_root->string());
        } else {
            spdlog::info("Serving static UI from {}", web_root->string());
        }

        m_server.Get("/", [](const httplib::Request&, httplib::Response& res) { res.set_redirect("/index.html"); });
    } else {
        spdlog::warn("Static UI not found. Expected src/adapters/ui/web/static relative to project root.");
    }

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

    m_server.Post(R"(/containers/([^/]+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        if (req.matches.size() < 2) {
            res.status = 400;
            res.set_content(R"({"error":"Missing container id"})", "application/json");
            return;
        }

        const std::string container_id = req.matches[1];
        try {
            spdlog::info("/containers/{}/stop requested", container_id);
            m_engine->stopContainer(container_id);
            json j;
            j["status"] = "ok";
            j["action"] = "stop";
            j["id"] = container_id;
            res.set_content(j.dump(4), "application/json");
        } catch (const std::exception& ex) {
            json err;
            err["error"] = ex.what();
            res.status = 500;
            res.set_content(err.dump(4), "application/json");
            spdlog::error("/containers/{}/stop failed: {}", container_id, ex.what());
        }
    });

    m_server.Post(R"(/containers/([^/]+)/kill)", [this](const httplib::Request& req, httplib::Response& res) {
        if (req.matches.size() < 2) {
            res.status = 400;
            res.set_content(R"({"error":"Missing container id"})", "application/json");
            return;
        }

        const std::string container_id = req.matches[1];
        try {
            spdlog::info("/containers/{}/kill requested", container_id);
            m_engine->killContainer(container_id);
            json j;
            j["status"] = "ok";
            j["action"] = "kill";
            j["id"] = container_id;
            res.set_content(j.dump(4), "application/json");
        } catch (const std::exception& ex) {
            json err;
            err["error"] = ex.what();
            res.status = 500;
            res.set_content(err.dump(4), "application/json");
            spdlog::error("/containers/{}/kill failed: {}", container_id, ex.what());
        }
    });
}

}  // namespace chaos::adapters::ui::web
