#include "interfaces/web/Server.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "shared/StateBroadcaster.hpp"
#include "validation/ValidationEngine.hpp"

using json = nlohmann::json;

namespace chaos::orchestrator::interfaces::web {

namespace {

json stateToJson(const shared::TargetState& s, const std::string& phase = "") {
    json j;
    j["container_id"] = s.container_id;
    j["status"] = shared::toString(s.status);
    if (s.cpu_usage_percent) j["cpu_usage_percent"] = *s.cpu_usage_percent;
    if (s.memory_usage_mb) j["memory_usage_mb"] = *s.memory_usage_mb;
    if (s.container_ip) j["container_ip"] = *s.container_ip;
    j["recent_logs"] = s.recent_logs;
    if (s.network_rx_bytes) j["network_rx_bytes"] = *s.network_rx_bytes;
    if (s.network_tx_bytes) j["network_tx_bytes"] = *s.network_tx_bytes;
    if (!phase.empty()) j["phase"] = phase;
    return j;
}

json limitsToJson(const containers::SystemInfo& info) {
    json j;
    j["cpu_cores"] = std::thread::hardware_concurrency();
    auto memoryTotalMb = static_cast<uint64_t>(info.memTotal / (1024 * 1024));
    j["memory_total_mb"] = memoryTotalMb;
    j["perturbation_limits"] = {
        {"cpu_cap", {{"min_percent", 1}, {"max_percent", 100}}},
        {"memory_cap", {{"min_mb", 1}, {"max_mb", memoryTotalMb}}},
        {"network_delay", {{"min_ms", 0}, {"max_ms", 30000}}},
    };
    return j;
}

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
            err["error"] = "Failed to list containers";
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
            err["error"] = "Failed to stop container";
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
            err["error"] = "Failed to kill container";
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
            err["error"] = "Failed to get logs";
            res.set_content(err.dump(4), "application/json");
            SPDLOG_ERROR("/containers/{}/logs failed: {}", id, ex.what());
        }
    });

    // --- New API endpoints ---

    m_server.Get("/api/targets",
                 [this](const httplib::Request& req, httplib::Response& res) { handleTargets(req, res); });

    m_server.Get("/api/limits",
                 [this](const httplib::Request& req, httplib::Response& res) { handleLimits(req, res); });

    m_server.Post("/api/run", [this](const httplib::Request& req, httplib::Response& res) { handleRun(req, res); });

    m_server.Post("/api/run/abort",
                  [this](const httplib::Request& req, httplib::Response& res) { handleAbort(req, res); });

    m_server.Get("/events", [this](const httplib::Request& req, httplib::Response& res) { handleEvents(req, res); });

    // --- Legacy synchronous POST /run (unchanged) ---

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
            SPDLOG_INFO("Executing manifest '{}' against target '{}'", manifest.test_name, manifest.target.id);

            perturbations::PerturbationFactory factory;
            std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbation_instances;
            for (const auto& spec : manifest.perturbations) {
                perturbation_instances.push_back(factory.create(m_engine, manifest.target, spec));
            }

            perturbations::PerturbationEngine pert_engine;
            const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));
            pert_engine.scheduleAllAsync(std::move(perturbation_instances), duration);

            observability::ObservabilityEngine obs(m_engine);
            shared::TargetState lastKnownState;
            shared::StateBroadcaster broadcaster;

            if (duration.count() > 0) {
                SPDLOG_INFO("Injecting faults for {}s. Monitoring real-time state...", duration.count());
                auto start_time = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() - start_time < duration) {
                    lastKnownState = obs.observe(manifest.target.id);
                    broadcaster.broadcast(lastKnownState);
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            } else {
                lastKnownState = obs.observe(manifest.target.id);
                broadcaster.broadcast(lastKnownState);
            }

            pert_engine.waitForTeardown();

            const auto results = validation::validate(lastKnownState, manifest.expectations);

            bool passed = std::ranges::all_of(results, [](const auto& r) { return r.passed; });
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
            err["error"] = "Invalid manifest";
            res.set_content(err.dump(4), "application/json");
            SPDLOG_ERROR("/run manifest parse failed: {}", ex.what());
        } catch (const std::exception& ex) {
            res.status = 500;
            json err;
            err["error"] = "Internal server error";
            res.set_content(err.dump(4), "application/json");
            SPDLOG_ERROR("/run failed: {}", ex.what());
        }
    });
}

void Server::handleTargets(const httplib::Request&, httplib::Response& res) {
    try {
        auto containers = m_engine->listContainers();
        json j = json::array();
        for (const auto& c : containers) {
            j.push_back({{"id", c.id}, {"name", c.name}, {"state", c.state}});
        }
        res.set_content(j.dump(4), "application/json");
        SPDLOG_INFO("/api/targets served: {} items", containers.size());
    } catch (const std::exception& ex) {
        json err;
        err["error"] = "Failed to list containers";
        res.status = 500;
        res.set_content(err.dump(4), "application/json");
        SPDLOG_ERROR("/api/targets failed: {}", ex.what());
    }
}

void Server::handleLimits(const httplib::Request&, httplib::Response& res) {
    try {
        auto info = m_engine->getSystemInfo();
        json j = limitsToJson(info);
        res.set_content(j.dump(4), "application/json");
        SPDLOG_INFO("/api/limits served");
    } catch (const std::exception& ex) {
        json err;
        err["error"] = "Failed to get system info";
        res.status = 500;
        res.set_content(err.dump(4), "application/json");
        SPDLOG_ERROR("/api/limits failed: {}", ex.what());
    }
}

void Server::handleRun(const httplib::Request& req, httplib::Response& res) {
    if (req.body.empty()) {
        res.status = 400;
        json err;
        err["error"] = "Empty request body";
        res.set_content(err.dump(4), "application/json");
        return;
    }

    try {
        auto manifest = manifests::ManifestParser::parseFromJson(req.body);
        SPDLOG_INFO("Executing manifest '{}' against target '{}' (async)", manifest.test_name, manifest.target.id);

        auto session = std::make_shared<RunSession>();
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            m_session = session;
            session->running = true;
        }

        std::thread([engine = m_engine, session, manifest = std::move(manifest)]() {
            try {
                perturbations::PerturbationFactory factory;
                std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbation_instances;
                for (const auto& spec : manifest.perturbations) {
                    perturbation_instances.push_back(factory.create(engine, manifest.target, spec));
                }

                observability::ObservabilityEngine obs(engine);
                shared::TargetState lastKnownState;

                auto test_start = std::chrono::steady_clock::now();
                {
                    std::lock_guard<std::mutex> lock(session->mtx);
                    session->phase = "normal";
                }

                // Quick baseline: 2 seconds (4 ticks)
                for (int i = 0; i < 4; ++i) {
                    auto ts = obs.observe(manifest.target.id);
                    {
                        std::lock_guard<std::mutex> lock(session->mtx);
                        session->latest = ts;
                    }
                    session->cv.notify_all();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                perturbations::PerturbationEngine pert_engine;
                const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));
                pert_engine.scheduleAllAsync(std::move(perturbation_instances), duration);

                {
                    std::lock_guard<std::mutex> lock(session->mtx);
                    session->phase = "chaos";
                }

                if (duration.count() > 0) {
                    SPDLOG_INFO("Injecting faults for {}s. Monitoring real-time state...", duration.count());
                    while (std::chrono::steady_clock::now() - test_start < duration + std::chrono::seconds(2)) {
                        lastKnownState = obs.observe(manifest.target.id);
                        {
                            std::lock_guard<std::mutex> lock(session->mtx);
                            session->latest = lastKnownState;
                        }
                        session->cv.notify_all();
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }
                } else {
                    lastKnownState = obs.observe(manifest.target.id);
                    {
                        std::lock_guard<std::mutex> lock(session->mtx);
                        session->latest = lastKnownState;
                    }
                    session->cv.notify_all();
                }

                pert_engine.waitForTeardown();
                {
                    std::lock_guard<std::mutex> lock(session->mtx);
                    session->phase = "recovery";
                }
                session->cv.notify_all();

                const auto results = validation::validate(lastKnownState, manifest.expectations);

                bool passed = std::ranges::all_of(results, [](const auto& r) { return r.passed; });
                json j;
                j["passed"] = passed;
                j["results"] = json::array();
                for (const auto& r : results) {
                    j["results"].push_back({{"type", r.expectationType}, {"passed", r.passed}, {"message", r.message}});
                }

                {
                    std::lock_guard<std::mutex> lock(session->mtx);
                    if (!session->running) return;
                    session->results = j;
                    session->running = false;
                    session->complete = true;
                }
                session->cv.notify_all();
            } catch (const std::exception& ex) {
                {
                    std::lock_guard<std::mutex> lock(session->mtx);
                    if (!session->running) return;
                    session->error = ex.what();
                    session->running = false;
                    session->complete = true;
                }
                session->cv.notify_all();
                SPDLOG_ERROR("/api/run async failed: {}", ex.what());
            }
        }).detach();

        json resp;
        resp["status"] = "started";
        res.status = 202;
        res.set_content(resp.dump(4), "application/json");
        SPDLOG_INFO("/api/run started async execution for '{}'", manifest.test_name);

    } catch (const manifests::ManifestParserError& ex) {
        res.status = 400;
        json err;
        err["error"] = "Invalid manifest";
        res.set_content(err.dump(4), "application/json");
        SPDLOG_ERROR("/api/run manifest parse failed: {}", ex.what());
    } catch (const std::exception& ex) {
        res.status = 500;
        json err;
        err["error"] = "Internal server error";
        res.set_content(err.dump(4), "application/json");
        SPDLOG_ERROR("/api/run failed: {}", ex.what());
    }
}

void Server::handleEvents(const httplib::Request&, httplib::Response& res) {
    res.set_header("Cache-Control", "no-store");
    res.set_header("Connection", "keep-alive");

    std::shared_ptr<RunSession> session;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        session = m_session;
    }

    res.set_chunked_content_provider(
        "text/event-stream", [session](size_t /*offset*/, httplib::DataSink& sink) -> bool {
            if (!session) {
                if (!sink.write("event: error\ndata: {\"error\":\"No active run\"}\n\n", 47)) {
                    return false;
                }
                sink.done();
                return false;
            }

            std::unique_lock<std::mutex> lock(session->mtx);
            session->cv.wait(lock, [session]() { return session->latest.has_value() || session->complete; });

            if (session->latest.has_value()) {
                auto j = stateToJson(*session->latest, session->phase);
                std::string data = "event: state\ndata: " + j.dump() + "\n\n";
                if (!sink.write(data.data(), data.size())) {
                    return false;
                }
                session->latest.reset();
            }

            if (session->complete) {
                if (!session->error.empty()) {
                    json err;
                    err["error"] = session->error;
                    std::string data = "event: error\ndata: " + err.dump() + "\n\n";
                    if (!sink.write(data.data(), data.size())) {
                        return false;
                    }
                } else {
                    std::string data = "event: complete\ndata: " + session->results.dump() + "\n\n";
                    if (!sink.write(data.data(), data.size())) {
                        return false;
                    }
                }
                sink.done();
                return false;
            }

            return true;
        });
}

void Server::handleAbort(const httplib::Request&, httplib::Response& res) {
    std::shared_ptr<RunSession> session;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (!m_session || !m_session->running) {
            json j = {{"error", "No active run to abort"}};
            res.status = 409;
            res.set_content(j.dump(4), "application/json");
            return;
        }
        session = m_session;
    }
    {
        std::lock_guard<std::mutex> lock(session->mtx);
        session->error = "Aborted by user";
        session->complete = true;
        session->running = false;
    }
    session->cv.notify_all();
    json j = {{"status", "aborted"}};
    res.set_content(j.dump(4), "application/json");
}

}  // namespace chaos::orchestrator::interfaces::web
