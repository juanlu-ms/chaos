#include "interfaces/web/Server.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "core/IRunObserver.hpp"
#include "core/ObservationLoop.hpp"
#include "core/RunOrchestrator.hpp"
#include "manifests/ManifestParser.hpp"
#include "web/JsonSerializer.hpp"

using json = nlohmann::json;

namespace chaos::orchestrator::interfaces::web {

namespace {

class WebRunObserver final : public core::IRunObserver {
    std::shared_ptr<core::RunSession> session_;

public:
    explicit WebRunObserver(std::shared_ptr<core::RunSession> session) : session_(std::move(session)) {}

    void onStateUpdate(const shared::TargetState& state) override {
        std::lock_guard lock(session_->mtx);
        session_->latest = state;
        session_->cv.notify_all();
    }

    void onPhaseChange(std::string_view phase) override {
        std::lock_guard lock(session_->mtx);
        session_->phase = std::string(phase);
        session_->cv.notify_all();
    }

    void onLogsUpdate(const std::vector<std::string>& logs) override {
        std::lock_guard lock(session_->mtx);
        session_->pending_logs = logs;
        session_->cv.notify_all();
    }
};

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

json runResultToJson(const core::RunResult& runResult) {
    json result;
    result["passed"] = runResult.passed;
    result["results"] = json::array();
    for (const auto& vr : runResult.results) {
        result["results"].push_back({{"type", vr.expectationType}, {"passed", vr.passed}, {"message", vr.message}});
    }
    return result;
}

void storeRunResult(const std::shared_ptr<core::RunSession>& session, json result) {
    std::lock_guard lock(session->mtx);
    if (!session->running) return;
    session->results = std::move(result);
    session->running = false;
    session->complete = true;
}

void storeRunError(const std::shared_ptr<core::RunSession>& session, const std::string& error) {
    std::lock_guard lock(session->mtx);
    if (!session->running) {
        return;
    }
    session->error = error;
    session->running = false;
    session->complete = true;
}
}  // namespace

Server::Server(std::shared_ptr<containers::IContainerEngine> engine) : engine_(std::move(engine)), runner_(engine_) {
    setupRoutes();
}

void Server::listen(int port) {
    SPDLOG_INFO("chaos listening to http://127.0.0.1:{}", port);
    if (!server_.listen("127.0.0.1", port)) {
        throw std::system_error(errno, std::generic_category(), "Failed to bind to port " + std::to_string(port));
    }
}

void Server::setupRoutes() {
    if (auto webRoot = findWebRoot(); webRoot.has_value()) {
        if (!server_.set_mount_point("/", webRoot->string())) {
            SPDLOG_WARN("Failed to mount static UI from {}", webRoot->string());
        } else {
            SPDLOG_INFO("Serving static UI from {}", webRoot->string());
        }

        server_.Get("/",
                    [](const httplib::Request&, httplib::Response& response) { response.set_redirect("/index.html"); });
    } else {
        SPDLOG_WARN("Static UI not found. Expected orchestrator/src/interfaces/web/static relative to project root.");
    }

    setupApiRoutes();
}

void Server::setupApiRoutes() {
    server_.Get("/api/status", [](const httplib::Request&, httplib::Response& response) {
        json result;
        result["project"] = "Chaos Engine";
        result["status"] = "Online (Web Adapter)";

        response.set_content(result.dump(4), "application/json");
    });

    server_.Get("/api/containers", [this](const httplib::Request& request, httplib::Response& response) {
        handleTargets(request, response);
    });

    server_.Post(R"(/api/containers/([^/]+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        handleContainerAction(req, res, "stop");
    });

    server_.Post(R"(/api/containers/([^/]+)/kill)", [this](const httplib::Request& req, httplib::Response& res) {
        handleContainerAction(req, res, "kill");
    });

    server_.Get(R"(/api/containers/([^/]+)/logs)",
                [this](const httplib::Request& request, httplib::Response& response) {
                    if (request.matches.size() < 2) {
                        response.status = 400;
                        response.set_content("Missing container id", "text/plain");
                        return;
                    }
                    const std::string id = request.matches[1];
                    try {
                        const auto logs = engine_->getLogs(id);
                        response.set_content(logs, "text/plain");
                    } catch (const std::exception& ex) {
                        response.status = 500;
                        json error;
                        error["error"] = "Failed to get logs";
                        response.set_content(error.dump(4), "application/json");
                        SPDLOG_ERROR("/api/containers/{}/logs failed: {}", id, ex.what());
                    }
                });

    server_.Get("/api/limits", [this](const httplib::Request& request, httplib::Response& response) {
        handleLimits(request, response);
    });

    server_.Post("/api/run", [this](const httplib::Request& request, httplib::Response& response) {
        handleRun(request, response);
    });

    server_.Post("/api/run/abort", [this](const httplib::Request& request, httplib::Response& response) {
        handleAbort(request, response);
    });

    server_.Get("/api/events", [this](const httplib::Request& request, httplib::Response& response) {
        handleEvents(request, response);
    });
}

void Server::handleContainerAction(const httplib::Request& req, httplib::Response& res, std::string_view actionName) {
    if (req.matches.size() < 2) {
        res.status = 400;
        res.set_content(R"({"error":"Missing container id"})", "application/json");
        return;
    }

    const std::string containerId = req.matches[1];
    try {
        SPDLOG_INFO("/api/containers/{}/{} requested", containerId, actionName);

        if (actionName == "stop") {
            engine_->stopContainer(containerId);
        } else if (actionName == "kill") {
            engine_->killContainer(containerId);
        }

        json result;
        result["status"] = "ok";
        result["action"] = std::string(actionName);
        result["id"] = containerId;
        res.set_content(result.dump(4), "application/json");
    } catch (const std::exception& ex) {
        json error;
        error["error"] = "Failed to " + std::string(actionName) + " container";
        res.status = 500;
        res.set_content(error.dump(4), "application/json");
        SPDLOG_ERROR("/api/containers/{}/{} failed: {}", containerId, actionName, ex.what());
    }
}

void Server::executeRunAsync(manifests::ChaosManifest manifest, std::shared_ptr<core::RunSession> session) {
    run_thread_ = std::jthread(
        [this, engine = engine_, session, manifest = std::move(manifest)](const std::stop_token& token) mutable {
            if (token.stop_requested()) {
                return;
            }
            try {
                WebRunObserver observer(session);

                core::ObservationLoop::Config loopConfig;
                loopConfig.metricsInterval = std::chrono::milliseconds(100);
                loopConfig.logsInterval = std::chrono::milliseconds(1500);
                loopConfig.continuousExpectations = manifest.expectations;

                core::ObservationLoop obsLoop(engine, manifest.target.id, session->state, observer, loopConfig);
                obsLoop.start(token);

                core::RunOrchestrator orchestrator(runner_);
                auto perturbation_instances = runner_.buildPerturbations(manifest);
                auto runResult =
                    orchestrator.run(manifest, session->state, observer, std::move(perturbation_instances), token);

                auto result = runResultToJson(runResult);
                storeRunResult(session, std::move(result));
                session->cv.notify_all();
            } catch (const std::exception& ex) {
                storeRunError(session, ex.what());
                session->cv.notify_all();
                SPDLOG_ERROR("/api/run async failed: {}", ex.what());
            }
        });
}

void Server::handleRun(const httplib::Request& request, httplib::Response& response) {
    if (request.body.empty()) {
        response.status = 400;
        json error;
        error["error"] = "Empty request body";
        response.set_content(error.dump(4), "application/json");
        return;
    }

    try {
        auto manifest = manifests::ManifestParser::parseFromJson(request.body);
        SPDLOG_INFO("Executing manifest '{}' against target '{}' (async)", manifest.test_name, manifest.target.id);

        auto session = std::make_shared<RunSession>();
        {
            std::lock_guard lock(session_mutex_);
            session_.reset();
            session_ = session;
            session->running = true;
        }

        if (run_thread_.joinable()) {
            run_thread_.request_stop();
            run_thread_.join();
        }

        executeRunAsync(std::move(manifest), std::move(session));

        json resp;
        resp["status"] = "started";
        response.status = 202;
        response.set_content(resp.dump(4), "application/json");
        SPDLOG_INFO("/api/run started async execution");

    } catch (const manifests::ManifestParserError& ex) {
        response.status = 400;
        json error;
        error["error"] = "Invalid manifest";
        response.set_content(error.dump(4), "application/json");
        SPDLOG_ERROR("/api/run manifest parse failed: {}", ex.what());
    } catch (const std::exception& ex) {
        response.status = 500;
        json error;
        error["error"] = "Internal server error";
        response.set_content(error.dump(4), "application/json");
        SPDLOG_ERROR("/api/run failed: {}", ex.what());
    }
}

void Server::handleEvents(const httplib::Request&, httplib::Response& response) {
    response.set_header("Cache-Control", "no-store");
    response.set_header("Connection", "keep-alive");

    std::shared_ptr<RunSession> session;
    bool wasCompleteAtConnect = false;
    {
        std::lock_guard lock(session_mutex_);
        session = session_;
        wasCompleteAtConnect = session ? session->complete : false;
    }

    response.set_chunked_content_provider(
        "text/event-stream", [this, session, wasCompleteAtConnect](size_t /*offset*/, httplib::DataSink const& sink) {
            if (!session || wasCompleteAtConnect) {
                if (!sink.write("event: error\ndata: {\"error\":\"No active run\"}\n\n", 47)) {
                    return false;
                }
                sink.done();
                return false;
            }

            std::unique_lock lock(session->mtx);
            session->cv.wait_for(lock, std::chrono::milliseconds(100),
                                 [session]() { return session->latest.has_value() || session->complete; });

            if (session->latest.has_value()) {
                if (!session->pending_logs.empty()) {
                    session->latest->recent_logs = std::move(session->pending_logs);
                    session->pending_logs.clear();
                }
                auto stateJson = stateToJson(*session->latest, session->phase, session->state.continuousFailures());
                writeSSEEvent(sink, stateJson);
                session->latest.reset();
            }

            if (session->complete) {
                if (!session->error.empty()) {
                    json error;
                    error["error"] = session->error;
                    std::string data = "event: error\ndata: " + error.dump() + "\n\n";
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

void Server::handleTargets(const httplib::Request&, httplib::Response& response) {
    try {
        auto containers = engine_->listContainers();
        json result = json::array();
        for (const auto& container : containers) {
            result.push_back({{"id", container.id}, {"name", container.name}, {"state", container.state}});
        }
        response.set_content(result.dump(4), "application/json");
        SPDLOG_INFO("/api/containers served: {} items", containers.size());
    } catch (const std::exception& ex) {
        json error;
        error["error"] = "Failed to list containers";
        response.status = 500;
        response.set_content(error.dump(4), "application/json");
        SPDLOG_ERROR("/api/containers failed: {}", ex.what());
    }
}

void Server::handleLimits(const httplib::Request&, httplib::Response& response) {
    try {
        auto info = engine_->getSystemInfo();
        json result = limitsToJson(info);
        response.set_content(result.dump(4), "application/json");
        SPDLOG_INFO("/api/limits served");
    } catch (const std::exception& ex) {
        json error;
        error["error"] = "Failed to get system info";
        response.status = 500;
        response.set_content(error.dump(4), "application/json");
        SPDLOG_ERROR("/api/limits failed: {}", ex.what());
    }
}

void Server::handleAbort(const httplib::Request&, httplib::Response& response) {
    std::shared_ptr<RunSession> session;
    {
        std::lock_guard lock(session_mutex_);
        if (!session_ || !session_->running) {
            json error = {{"error", "No active run to abort"}};
            response.status = 409;
            response.set_content(error.dump(4), "application/json");
            return;
        }
        session = session_;
    }
    {
        std::lock_guard lock(session->mtx);
        session->error = "Aborted by user";
        session->complete = true;
        session->running = false;
    }
    session->stop_source.request_stop();
    run_thread_.request_stop();
    session->cv.notify_all();
    json result = {{"status", "aborted"}};
    response.set_content(result.dump(4), "application/json");
}

void Server::writeSSEEvent(const httplib::DataSink& sink, const nlohmann::json& data) {
    std::string event = "event: state\ndata: " + data.dump() + "\n\n";
    sink.write(event.data(), event.size());
}

}  // namespace chaos::orchestrator::interfaces::web
