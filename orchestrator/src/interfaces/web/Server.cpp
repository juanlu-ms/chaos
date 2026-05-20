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

#include "containers/internal/CgroupMetricsGatherer.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "validation/ValidationEngine.hpp"
#include "web/JsonSerializer.hpp"

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

Server::Server(std::shared_ptr<containers::IContainerEngine> engine) : engine_(std::move(engine)), runner_(engine_) {
    setupRoutes();
}

void Server::listen(int port) {
    SPDLOG_INFO("chaos listening to http://127.0.0.1:{}", port);
    server_.listen("127.0.0.1", port);
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

    server_.Get("/status", [](const httplib::Request&, httplib::Response& response) {
        json result;
        result["project"] = "Chaos Engine";
        result["status"] = "Online (Web Adapter)";

        response.set_content(result.dump(4), "application/json");
    });

    server_.Get("/containers", [this](const httplib::Request&, httplib::Response& response) {
        try {
            SPDLOG_DEBUG("/containers requested");
            auto containers = engine_->listContainers();
            json result = json::array();
            for (const auto& container : containers) {
                result.push_back({{"id", container.id}, {"name", container.name}, {"state", container.state}});
            }
            response.set_content(result.dump(4), "application/json");
            SPDLOG_INFO("/containers served: {} items", containers.size());
        } catch (const std::exception& ex) {
            json error;
            error["error"] = "Failed to list containers";
            response.status = 500;
            response.set_content(error.dump(4), "application/json");
            SPDLOG_ERROR("/containers failed: {}", ex.what());
        }
    });

    server_.Post(R"(/containers/([^/]+)/stop)", [this](const httplib::Request& request, httplib::Response& response) {
        if (request.matches.size() < 2) {
            response.status = 400;
            response.set_content(R"({"error":"Missing container id"})", "application/json");
            return;
        }

        const std::string containerId = request.matches[1];
        try {
            SPDLOG_INFO("/containers/{}/stop requested", containerId);
            engine_->stopContainer(containerId);
            json result;
            result["status"] = "ok";
            result["action"] = "stop";
            result["id"] = containerId;
            response.set_content(result.dump(4), "application/json");
        } catch (const std::exception& ex) {
            json error;
            error["error"] = "Failed to stop container";
            response.status = 500;
            response.set_content(error.dump(4), "application/json");
            SPDLOG_ERROR("/containers/{}/stop failed: {}", containerId, ex.what());
        }
    });

    server_.Post(R"(/containers/([^/]+)/kill)", [this](const httplib::Request& request, httplib::Response& response) {
        if (request.matches.size() < 2) {
            response.status = 400;
            response.set_content(R"({"error":"Missing container id"})", "application/json");
            return;
        }

        const std::string containerId = request.matches[1];
        try {
            SPDLOG_INFO("/containers/{}/kill requested", containerId);
            engine_->killContainer(containerId);
            json result;
            result["status"] = "ok";
            result["action"] = "kill";
            result["id"] = containerId;
            response.set_content(result.dump(4), "application/json");
        } catch (const std::exception& ex) {
            json error;
            error["error"] = "Failed to kill container";
            response.status = 500;
            response.set_content(error.dump(4), "application/json");
            SPDLOG_ERROR("/containers/{}/kill failed: {}", containerId, ex.what());
        }
    });

    server_.Get(R"(/containers/([^/]+)/logs)", [this](const httplib::Request& request, httplib::Response& response) {
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
            SPDLOG_ERROR("/containers/{}/logs failed: {}", id, ex.what());
        }
    });

    server_.Get("/api/targets", [this](const httplib::Request& request, httplib::Response& response) {
        handleTargets(request, response);
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

    server_.Get("/events", [this](const httplib::Request& request, httplib::Response& response) {
        handleEvents(request, response);
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
        SPDLOG_INFO("/api/targets served: {} items", containers.size());
    } catch (const std::exception& ex) {
        json error;
        error["error"] = "Failed to list containers";
        response.status = 500;
        response.set_content(error.dump(4), "application/json");
        SPDLOG_ERROR("/api/targets failed: {}", ex.what());
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

        {
            std::lock_guard lock(session_mutex_);
            session_.reset();
        }

        auto session = std::make_shared<RunSession>();
        {
            std::lock_guard lock(session_mutex_);
            session_ = session;
            session->running = true;
        }

        auto perturbation_instances = runner_.buildPerturbations(manifest);

        std::thread([engine = engine_, session, manifest = std::move(manifest),
                     perturbation_instances = std::move(perturbation_instances)]() mutable {
            try {
                using namespace std::chrono;
                using namespace std::chrono_literals;

                observability::ObservabilityEngine obs(engine);
                shared::TargetState lastKnownState;

                // Fetch IP once at setup.
                auto ip = obs.getContainerIp(manifest.target.id);
                if (ip) {
                    lastKnownState.container_ip = ip;
                }

                // Phase manager — runs on the calling thread.
                {
                    std::lock_guard lock(session->mtx);
                    session->phase = "normal";
                }

                // cgroup-based metrics (bypasses Docker API).
                containers::internal::CgroupMetricsGatherer cgroup;
                const bool useCgroup =
                    containers::internal::CgroupMetricsGatherer::resolveCgroupPath(manifest.target.id).has_value();
                if (useCgroup) {
                    SPDLOG_INFO("Using cgroup v2 for CPU/memory metrics");
                }

                // ═══════════════════════════════════════════════════════
                //  Three independent background workers — each polls its
                //  own data source at its own rate and pushes updates to
                //  the SSE handler via session->latest.
                // ═══════════════════════════════════════════════════════

                // ── Worker 1: Metrics (CPU + memory + status) ────────
                std::jthread metricsWorker([&](const std::stop_token& stop_token) {
                    while (!stop_token.stop_requested() && session->running) {
                        auto tick = steady_clock::now();
                        auto state = lastKnownState;

                        state.status = obs.getStatus(manifest.target.id);
                        if (state.status == shared::ContainerStatus::Running) {
                            if (useCgroup) {
                                auto cpu = cgroup.getCpuUsagePercent(manifest.target.id);
                                auto mem = cgroup.getMemoryUsageMb(manifest.target.id);
                                if (cpu) {
                                    state.cpu_usage_percent = cpu;
                                }
                                if (mem) {
                                    state.memory_usage_mb = mem;
                                }
                            } else {
                                try {
                                    auto stats = obs.getStats(manifest.target.id);
                                    state.cpu_usage_percent = stats.cpu_percent;
                                    state.memory_usage_mb = stats.memory_mb;
                                    state.network_rx_bps = stats.network_rx_bps;
                                    state.network_tx_bps = stats.network_tx_bps;
                                } catch (const containers::ContainerEngineError& e) {
                                    SPDLOG_ERROR("Metrics worker: failed to fetch stats: {}", e.what());
                                }
                            }
                        }
                        {
                            std::lock_guard lock(session->mtx);
                            session->latest = std::move(state);
                        }
                        session->cv.notify_all();

                        auto elapsed = steady_clock::now() - tick;
                        auto remaining = 100ms - elapsed;
                        if (remaining > 0ms) {
                            std::this_thread::sleep_for(remaining);
                        }
                    }
                });

                // ── Worker 2: Logs ───────────────────────────────────
                std::jthread logsWorker([&](const std::stop_token& stop_token) {
                    while (!stop_token.stop_requested() && session->running) {
                        auto tick = steady_clock::now();
                        auto rawLogs = obs.getLogs(manifest.target.id);
                        std::vector<std::string> logLines;
                        parseLogLines(rawLogs, logLines);
                        {
                            std::lock_guard lock(session->mtx);
                            session->pending_logs = std::move(logLines);
                        }
                        session->cv.notify_all();

                        auto elapsed = steady_clock::now() - tick;
                        auto remaining = 1500ms - elapsed;
                        if (remaining > 0ms) {
                            std::this_thread::sleep_for(remaining);
                        }
                    }
                });

                // ═══════════════════════════════════════════════════════
                //  Phase manager — runs on the calling thread.
                // ═══════════════════════════════════════════════════════

                // Baseline: 2 seconds of "normal" phase.
                std::this_thread::sleep_for(2s);
                // Start fault injection.
                perturbations::PerturbationEngine pert_engine;
                const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));
                if (duration.count() > 0) {
                    auto session_stop = session->stop_source.get_token();
                    pert_engine.scheduleAllAsync(std::move(perturbation_instances), duration, session_stop);
                    SPDLOG_INFO("Injecting faults for {}s.", duration.count());
                    {
                        std::lock_guard lock(session->mtx);
                        session->phase = "chaos";
                    }
                    session->cv.notify_all();

                    auto chaos_end = std::chrono::steady_clock::now() + duration + 2s;
                    while (std::chrono::steady_clock::now() < chaos_end) {
                        if (session_stop.stop_requested()) {
                            pert_engine.cancel();
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }

                // Teardown and recovery.
                pert_engine.waitForTeardown();
                {
                    std::lock_guard lock(session->mtx);
                    if (session->latest.has_value()) {
                        session->latest->container_ip = lastKnownState.container_ip;
                    } else {
                        session->latest = lastKnownState;
                    }
                    session->phase = "recovery";
                }
                session->cv.notify_all();

                // Validation needs an authoritative final snapshot.
                lastKnownState = obs.observe(manifest.target.id);
                if (lastKnownState.container_ip) {
                    lastKnownState.container_ip = ip;
                }

                // Use ChaosRunner for validation (reuse engine from outer scope).
                core::ChaosRunner validationRunner(engine);
                bool passed = validationRunner.validateExpectations(manifest, lastKnownState);

                json result;
                result["passed"] = passed;
                result["results"] = json::array();
                // For detailed results, still call validate directly.
                const auto validationResults = validation::validate(lastKnownState, manifest.expectations);
                for (const auto& vr : validationResults) {
                    result["results"].push_back(
                        {{"type", vr.expectationType}, {"passed", vr.passed}, {"message", vr.message}});
                }

                {
                    std::lock_guard lock(session->mtx);
                    if (!session->running) {
                        return;
                    }
                    session->results = result;
                    session->running = false;
                    session->complete = true;
                }
                session->cv.notify_all();

                // jthread destructors call request_stop() + join() here.
            } catch (const std::exception& ex) {
                {
                    std::lock_guard lock(session->mtx);
                    if (!session->running) {
                        return;
                    }
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
        response.status = 202;
        response.set_content(resp.dump(4), "application/json");
        SPDLOG_INFO("/api/run started async execution for '{}'", manifest.test_name);

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
    {
        std::lock_guard lock(session_mutex_);
        session = session_;
    }

    response.set_chunked_content_provider(
        "text/event-stream", [session, wasCompleteAtConnect = session ? session->complete : false](
                                 size_t /*offset*/, httplib::DataSink const& sink) {
            if (!session) {
                if (!sink.write("event: error\ndata: {\"error\":\"No active run\"}\n\n", 47)) {
                    return false;
                }
                sink.done();
                return false;
            }

            if (wasCompleteAtConnect) {
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
                auto stateJson = stateToJson(*session->latest, session->phase);
                if (std::string data = "event: state\ndata: " + stateJson.dump() + "\n\n";
                    !sink.write(data.data(), data.size())) {
                    return false;
                }
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
    session->cv.notify_all();
    json result = {{"status", "aborted"}};
    response.set_content(result.dump(4), "application/json");
}

}  // namespace chaos::orchestrator::interfaces::web
