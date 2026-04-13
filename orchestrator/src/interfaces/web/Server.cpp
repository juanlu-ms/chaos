#include <spdlog/spdlog.h>

#include <filesystem>
#include <interfaces/web/Server.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "validation/ValidationEngine.hpp"

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

Server::Server(std::shared_ptr<containers::IContainerEngine> engine) : m_engine(std::move(engine)) { setupRoutes(); }

void Server::listen(int port) {
    SPDLOG_INFO("chaos listening to http://127.0.0.1:{}", port);
    m_server.listen("127.0.0.1", port);
}

void Server::setupRoutes() {
    if (auto webRoot = findWebRoot(); webRoot.has_value()) {
        if (!m_server.set_mount_point("/", webRoot->string())) {
            SPDLOG_WARN("Failed to mount static UI from {}", webRoot->string());
        } else {
            SPDLOG_INFO("Serving static UI from {}", webRoot->string());
        }

        m_server.Get("/", [](const httplib::Request&, httplib::Response& res) { res.set_redirect("/index.html"); });
    } else {
        SPDLOG_WARN("Static UI not found. Expected orchestrator/src/interfaces/web/static relative to project root.");
    }

    m_server.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        json j;
        j["project"] = "Chaos Engine";
        j["status"] = "Online (Web Adapter)";

        res.set_content(j.dump(4), "application/json");
    });

    m_server.Get("/containers", [this](const httplib::Request&, httplib::Response& res) {
        try {
            SPDLOG_DEBUG("/containers requested");
            auto containers = m_engine->listContainers();
            json j = json::array();
            for (const auto& c : containers) {
                j.push_back({{"id", c.id}, {"name", c.name}, {"state", c.state}});
            }
            res.set_content(j.dump(4), "application/json");
            SPDLOG_INFO("/containers served: {} items", containers.size());
        } catch (const std::exception& ex) {
            json err;
            err["error"] = ex.what();
            res.status = 500;
            res.set_content(err.dump(4), "application/json");
            SPDLOG_ERROR("/containers failed: {}", ex.what());
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
            SPDLOG_INFO("/containers/{}/stop requested", containerId);
            m_engine->stopContainer(containerId);
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
            SPDLOG_ERROR("/containers/{}/stop failed: {}", containerId, ex.what());
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
            SPDLOG_INFO("/containers/{}/kill requested", containerId);
            m_engine->killContainer(containerId);
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
            SPDLOG_ERROR("/containers/{}/kill failed: {}", containerId, ex.what());
        }
    });

    m_server.Get(R"(/containers/([^/]+)/logs)", [this](const httplib::Request& req, httplib::Response& res) {
        if (req.matches.size() < 2) {
            res.status = 400;
            res.set_content("Missing container id", "text/plain");
            return;
        }
        const std::string id = req.matches[1];
        try {
            const auto logs = m_engine->getLogs(id);
            res.set_content(logs, "text/plain");
        } catch (const std::exception& ex) {
            res.status = 500;
            json err;
            err["error"] = ex.what();
            res.set_content(err.dump(4), "application/json");
        }
    });

    m_server.Post("/run", [this](const httplib::Request& req, httplib::Response& res) {
        if (req.body.empty()) {
            res.status = 400;
            json err;
            err["error"] = "Empty request body";
            res.set_content(err.dump(4), "application/json");
            return;
        }
        try {
            auto manifest = manifests::ManifestParser::parseFromJson(req.body);
            perturbations::PerturbationEngine pert_engine(m_engine);
            pert_engine.applyAll(manifest);

            observability::ObservabilityEngine obs(m_engine);
            validation::ValidationEngine validator(std::move(obs));
            const auto results = validator.validate(manifest.target.id, manifest.expectations);

            bool passed = std::all_of(results.begin(), results.end(), [](const auto& r) { return r.passed; });
            json j;
            j["passed"] = passed;
            j["results"] = json::array();
            for (const auto& r : results) {
                j["results"].push_back({{"type", r.expectationType}, {"passed", r.passed}, {"message", r.message}});
            }
            res.status = passed ? 200 : 422;
            res.set_content(j.dump(4), "application/json");
        } catch (const manifests::ManifestParserError& ex) {
            res.status = 400;
            json err;
            err["error"] = ex.what();
            res.set_content(err.dump(4), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            json err;
            err["error"] = ex.what();
            res.set_content(err.dump(4), "application/json");
        }
    });
}

}  // namespace chaos::orchestrator::interfaces::web
