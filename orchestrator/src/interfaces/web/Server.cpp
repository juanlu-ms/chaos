#include "interfaces/web/Server.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "core/CompositeRunObserver.hpp"
#include "core/IRunObserver.hpp"
#include "core/ObservationLoop.hpp"
#include "core/ResultSerializer.hpp"
#include "core/RunOrchestrator.hpp"
#include "history/HistoryJson.hpp"
#include "history/RunRecorder.hpp"
#include "interfaces/web/JsonSerializer.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/OtlpRunObserver.hpp"

using json = nlohmann::json;

namespace chaos::orchestrator::interfaces::web {

namespace {

class WebRunObserver final : public core::IRunObserver {
    std::shared_ptr<core::RunSession> session_;

public:
    explicit WebRunObserver(std::shared_ptr<core::RunSession> session) : session_(std::move(session)) {}

    void onStateUpdate(const core::TargetState& state) override {
        session_->state.updateMetrics(state);
        session_->cv.notify_all();
    }

    void onPhaseChange(std::string_view phase) override {
        session_->state.setPhase(phase);
        session_->cv.notify_all();
    }

    void onLogsUpdate(const std::vector<std::string>& logs) override {
        session_->state.updateLogs(logs);
        session_->cv.notify_all();
    }

    void onNetworkLatencyUpdate(std::optional<double> latency) override {
        session_->state.updateNetworkLatency(latency);
        session_->cv.notify_all();
    }
};

std::string_view mimeForExtension(std::string_view path) {
    auto const pos = path.rfind('.');
    if (pos == std::string_view::npos) return "application/octet-stream";
    auto const ext = path.substr(pos);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".js" || ext == ".mjs") return "text/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".woff") return "font/woff";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".ttf") return "font/ttf";
    if (ext == ".otf") return "font/otf";
    if (ext == ".wasm") return "application/wasm";
    if (ext == ".map") return "application/json; charset=utf-8";
    return "application/octet-stream";
}

void finalizeSession(const std::shared_ptr<core::RunSession>& session, json result, std::string error) {
    std::lock_guard lock(session->mtx);
    if (!result.is_null()) {
        session->results = std::move(result);
    }
    if (!error.empty()) {
        session->error = std::move(error);
    }
    session->running = false;
    session->complete = true;
}
}  // namespace

Server::Server(std::shared_ptr<containers::IContainerEngine> engine, std::shared_ptr<history::IRunHistory> history,
               std::optional<std::string> otlp_endpoint)
    : engine_(std::move(engine)),
      history_(std::move(history)),
      runner_(engine_),
      otlp_endpoint_(std::move(otlp_endpoint)) {
    setupRoutes();
}

Server::~Server() { server_.stop(); }

void Server::listen(int port) {
    SPDLOG_INFO("chaos listening to http://127.0.0.1:{}", port);
    if (!server_.listen("127.0.0.1", port)) {
        throw std::system_error(errno, std::generic_category(), fmt::format("Failed to bind to port {}", port));
    }
}

void Server::stop() { server_.stop(); }

void Server::setupRoutes() {
#ifdef CHAOS_HAVE_EMBEDDED_WEB_UI
    webFs_ = cmrc::chaos_web::get_filesystem();
    registerEmbeddedRoutes();
    SPDLOG_INFO("Serving embedded Web UI");
#else
    SPDLOG_INFO("Web UI not embedded — serving API only");
#endif

    setupApiRoutes();
}

#ifdef CHAOS_HAVE_EMBEDDED_WEB_UI
void Server::serveEmbedded(const httplib::Request& /*req*/, httplib::Response& res, const std::string& path) const {
    if (!webFs_ || !webFs_->is_file(path)) {
        res.status = 404;
        res.set_content("Not found", "text/plain");
        return;
    }
    auto file = webFs_->open(path);
    res.set_content(std::string(file.begin(), file.end()), std::string(mimeForExtension(path)));
}

void Server::registerEmbeddedRoutes() {
    server_.Get("/",
                [this](const httplib::Request& req, httplib::Response& res) { serveEmbedded(req, res, "index.html"); });
    server_.Get("/index.html",
                [this](const httplib::Request& req, httplib::Response& res) { serveEmbedded(req, res, "index.html"); });
    server_.Get(R"(/assets/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (req.matches.size() < 2) {
            res.status = 400;
            return;
        }
        serveEmbedded(req, res, "assets/" + req.matches[1].str());
    });
}
#endif

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

    server_.Get("/api/history",
                [this](const httplib::Request& req, httplib::Response& res) { handleHistoryList(req, res); });

    server_.Get(R"(/api/history/([^/]+))",
                [this](const httplib::Request& req, httplib::Response& res) { handleHistoryGet(req, res); });

    server_.Delete(R"(/api/history/([^/]+))",
                   [this](const httplib::Request& req, httplib::Response& res) { handleHistoryDelete(req, res); });

    server_.Delete("/api/history",
                   [this](const httplib::Request& req, httplib::Response& res) { handleHistoryClear(req, res); });
}

void Server::handleContainerAction(const httplib::Request& req, httplib::Response& res, std::string_view action_name) {
    if (req.matches.size() < 2) {
        res.status = 400;
        res.set_content(R"({"error":"Missing container id"})", "application/json");
        return;
    }

    const std::string container_id = req.matches[1];
    try {
        SPDLOG_INFO("/api/containers/{}/{} requested", container_id, action_name);

        if (action_name == "stop") {
            engine_->stopContainer(container_id);
        } else if (action_name == "kill") {
            engine_->killContainer(container_id);
        }

        json result;
        result["status"] = "ok";
        result["action"] = std::string(action_name);
        result["id"] = container_id;
        res.set_content(result.dump(4), "application/json");
    } catch (const std::exception& ex) {
        json error;
        error["error"] = "Failed to " + std::string(action_name) + " container";
        res.status = 500;
        res.set_content(error.dump(4), "application/json");
        SPDLOG_ERROR("/api/containers/{}/{} failed: {}", container_id, action_name, ex.what());
    }
}

void Server::executeRunAsync(manifests::ChaosManifest manifest, std::shared_ptr<core::RunSession> session) {
    run_thread_ = std::jthread([this, engine = engine_, session, manifest = std::move(manifest),
                                otlp_endpoint = otlp_endpoint_](const std::stop_token& token) mutable {
        if (token.stop_requested()) {
            return;
        }
        history::RunRecorder recorder(manifest);
        std::optional<observability::OtlpRunObserver> otlp_observer;
        try {
            WebRunObserver observer(session);
            if (otlp_endpoint.has_value()) {
                otlp_observer.emplace(observability::OtlpExporter(*otlp_endpoint));
            }
            std::vector<core::IRunObserver*> observer_list = {&observer, &recorder};
            if (otlp_observer.has_value()) {
                observer_list.push_back(&*otlp_observer);
            }
            core::CompositeRunObserver composite(observer_list);

            core::ObservationLoop::Config loop_config;
            loop_config.metricsInterval = std::chrono::milliseconds(100);
            loop_config.logsInterval = std::chrono::milliseconds(1500);
            loop_config.continuous_expectations = manifest.expectations;

            core::ObservationLoop obs_loop(engine, manifest.target.id, session->state, composite, loop_config);
            obs_loop.start(token);

            core::RunOrchestrator orchestrator(runner_);
            auto perturbation_instances = runner_.buildPerturbations(manifest);
            auto run_result =
                orchestrator.run(manifest, session->state, composite, std::move(perturbation_instances), token);

            auto result = core::runResultToJson(run_result);

            std::string status = session->abort_requested.load() ? "aborted" : "completed";
            if (history_) {
                history_->save(recorder.finalize(run_result, status, ""));
            }
            if (otlp_observer.has_value()) {
                (void)otlp_observer->finalize(run_result);
            }

            finalizeSession(session, std::move(result), {});
            session->cv.notify_all();
        } catch (const std::exception& ex) {
            if (history_) {
                history_->save(recorder.finalize({}, "error", ex.what()));
            }
            if (otlp_observer.has_value()) {
                (void)otlp_observer->finalize({});
            }
            finalizeSession(session, {}, std::string{ex.what()});
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
    bool was_complete_at_connect = false;
    uint64_t last_seq = 0;
    {
        std::lock_guard lock(session_mutex_);
        session = session_;
        was_complete_at_connect = session ? session->complete : false;
        if (session) {
            last_seq = session->state.sequence();
        }
    }

    response.set_chunked_content_provider(
        "text/event-stream",
        [this, session, was_complete_at_connect, last_seq](size_t /*offset*/, httplib::DataSink const& sink) mutable {
            SPDLOG_DEBUG("SSE event stream connected");
            if (!session || was_complete_at_connect) {
                SPDLOG_DEBUG("SSE event stream: no active session, closing");
                if (!sink.write("event: error\ndata: {\"error\":\"No active run\"}\n\n", 47)) {
                    return false;
                }
                sink.done();
                return false;
            }

            std::unique_lock lock(session->mtx);
            session->cv.wait_for(lock, std::chrono::milliseconds(100), [&session, &last_seq]() {
                return session->state.sequence() != last_seq || session->complete;
            });

            if (uint64_t new_seq = session->state.sequence(); new_seq != last_seq && !session->complete) {
                auto state = session->state.latestState();
                auto state_json = stateToJson(state, session->state.phase(), session->state.continuousFailures());
                writeSSEEvent(sink, state_json);
                last_seq = new_seq;
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
        session = session_;
    }
    if (!session) {
        json error = {{"error", "No active run to abort"}};
        response.status = 409;
        response.set_content(error.dump(4), "application/json");
        return;
    }
    session->abort_requested.store(true);
    run_thread_.request_stop();
    session->cv.notify_all();
    json result = {{"status", "aborted"}};
    response.set_content(result.dump(4), "application/json");
}

void Server::handleHistoryList(const httplib::Request&, httplib::Response& response) {
    if (!history_) {
        response.status = 404;
        json error = {{"error", "History not available"}};
        response.set_content(error.dump(4), "application/json");
        return;
    }
    auto summaries = history_->list();
    json result = json::array();
    for (const auto& s : summaries) {
        result.push_back(history::runSummaryToJson(s));
    }
    response.set_content(result.dump(4), "application/json");
}

void Server::handleHistoryGet(const httplib::Request& request, httplib::Response& response) {
    if (!history_) {
        response.status = 404;
        json error = {{"error", "History not available"}};
        response.set_content(error.dump(4), "application/json");
        return;
    }
    if (request.matches.size() < 2) {
        response.status = 400;
        json error = {{"error", "Missing run id"}};
        response.set_content(error.dump(4), "application/json");
        return;
    }
    const std::string id = request.matches[1];
    auto record = history_->get(id);
    if (!record.has_value()) {
        response.status = 404;
        json error = {{"error", "Run not found"}};
        response.set_content(error.dump(4), "application/json");
        return;
    }
    response.set_content(history::runRecordToJson(record.value()).dump(4), "application/json");
}

void Server::handleHistoryDelete(const httplib::Request& request, httplib::Response& response) {
    if (!history_) {
        response.status = 404;
        json error = {{"error", "History not available"}};
        response.set_content(error.dump(4), "application/json");
        return;
    }
    if (request.matches.size() < 2) {
        response.status = 400;
        json error = {{"error", "Missing run id"}};
        response.set_content(error.dump(4), "application/json");
        return;
    }
    const std::string id = request.matches[1];
    bool removed = history_->remove(id);
    if (!removed) {
        response.status = 404;
        json error = {{"error", "Run not found"}};
        response.set_content(error.dump(4), "application/json");
        return;
    }
    json result = {{"status", "deleted"}};
    response.set_content(result.dump(4), "application/json");
}

void Server::handleHistoryClear(const httplib::Request&, httplib::Response& response) {
    if (!history_) {
        response.status = 404;
        json error = {{"error", "History not available"}};
        response.set_content(error.dump(4), "application/json");
        return;
    }
    history_->clear();
    json result = {{"status", "cleared"}};
    response.set_content(result.dump(4), "application/json");
}

void Server::writeSSEEvent(const httplib::DataSink& sink, const nlohmann::json& data) {
    std::string event = "event: state\ndata: " + data.dump() + "\n\n";
    sink.write(event.data(), event.size());
}

}  // namespace chaos::orchestrator::interfaces::web
