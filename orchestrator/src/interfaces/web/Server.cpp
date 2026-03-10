#include <spdlog/spdlog.h>

#include <filesystem>
#include <interfaces/web/Server.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace chaos::orchestrator::interfaces::web {

namespace {
std::optional<std::filesystem::path> findWebRoot() {
    std::vector<std::filesystem::path> candidates = {
        "orchestrator/src/interfaces/web/static",
        "src/interfaces/web/static",
        "interfaces/web/static",
        "../orchestrator/src/interfaces/web/static",
        "../../orchestrator/src/interfaces/web/static",
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}
}  // namespace

Server::Server(chaos::orchestrator::containers::IContainerEngine& engine) : m_engine(engine) { setupRoutes(); }

void Server::listen(int port) {
    spdlog::info("chaos listening to http://127.0.0.1:{}", port);
    m_server.listen("127.0.0.1", port);
}

void Server::setupRoutes() {
    if (auto webRoot = findWebRoot(); webRoot.has_value()) {
        if (!m_server.set_mount_point("/", webRoot->string())) {
            spdlog::warn("Failed to mount static UI from {}", webRoot->string());
        } else {
            spdlog::info("Serving static UI from {}", webRoot->string());
        }

        m_server.Get("/", [](const httplib::Request&, httplib::Response& res) { res.set_redirect("/index.html"); });
    } else {
        spdlog::warn("Static UI not found. Expected orchestrator/src/interfaces/web/static relative to project root.");
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
            auto containers = m_engine.listContainers();
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

        const std::string containerId = req.matches[1];
        try {
            spdlog::info("/containers/{}/stop requested", containerId);
            m_engine.stopContainer(containerId);
            json j;
            j["status"] = "ok";
            j["action"] = "stop";
            j["id"] = containerId;
            res.set_content(j.dump(4), "application/json");
        } catch (const std::exception& ex) {
            json err;
            err["error"] = ex.what();
            res.status = 500;
            res.set_content(err.dump(4), "application/json");
            spdlog::error("/containers/{}/stop failed: {}", containerId, ex.what());
        }
    });

    m_server.Post(R"(/containers/([^/]+)/kill)", [this](const httplib::Request& req, httplib::Response& res) {
        if (req.matches.size() < 2) {
            res.status = 400;
            res.set_content(R"({"error":"Missing container id"})", "application/json");
            return;
        }

        const std::string containerId = req.matches[1];
        try {
            spdlog::info("/containers/{}/kill requested", containerId);
            m_engine.killContainer(containerId);
            json j;
            j["status"] = "ok";
            j["action"] = "kill";
            j["id"] = containerId;
            res.set_content(j.dump(4), "application/json");
        } catch (const std::exception& ex) {
            json err;
            err["error"] = ex.what();
            res.status = 500;
            res.set_content(err.dump(4), "application/json");
            spdlog::error("/containers/{}/kill failed: {}", containerId, ex.what());
        }
    });
}

}  // namespace chaos::orchestrator::interfaces::web
