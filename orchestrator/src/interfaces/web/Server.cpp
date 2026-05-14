#include "interfaces/web/Server.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
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
#include "perturbations/PerturbationFactory.hpp"
#include "validation/ValidationEngine.hpp"

using json = nlohmann::json;

namespace chaos::orchestrator::interfaces::web {

namespace {

void parseLogLines(const std::string& raw, std::vector<std::string>& out) {
    out.clear();
    if (raw.empty()) {
        return;
    }
    size_t start = 0;
    while (start < raw.size()) {
        size_t end = raw.find('\n', start);
        if (end == std::string::npos) {
            end = raw.size();
        }
        std::string line = raw.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            out.push_back(std::move(line));
        }
        start = end + 1;
    }
}

json stateToJson(const shared::TargetState& state, const std::string& phase = "") {
    json json;
    json["container_id"] = state.container_id;
    json["status"] = shared::toString(state.status);
    if (state.cpu_usage_percent) {
        json["cpu_usage_percent"] = *state.cpu_usage_percent;
    }
    if (state.memory_usage_mb) {
        json["memory_usage_mb"] = *state.memory_usage_mb;
    }
    if (state.container_ip) {
        json["container_ip"] = *state.container_ip;
    }
    json["recent_logs"] = state.recent_logs;
    if (state.network_rx_bps) {
        json["network_rx_bps"] = *state.network_rx_bps;
    }
    if (state.network_tx_bps) {
        json["network_tx_bps"] = *state.network_tx_bps;
    }
    if (!phase.empty()) {
        json["phase"] = phase;
    }
    return json;
}

json limitsToJson(const containers::SystemInfo& info) {
    json json;
    json["cpu_cores"] = std::thread::hardware_concurrency();
    auto memoryTotalMb = static_cast<uint64_t>(info.memTotal / (static_cast<int64_t>(1024 * 1024)));
    json["memory_total_mb"] = memoryTotalMb;
    json["perturbation_limits"] = {
        {"cpu_cap", {{"min_percent", 1}, {"max_percent", 100}}},
        {"memory_cap", {{"min_mb", 1}, {"max_mb", memoryTotalMb}}},
        {"network_delay", {{"min_ms", 0}, {"max_ms", 30000}}},
    };
    return json;
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

        m_server.Get(
            "/", [](const httplib::Request&, httplib::Response& response) { response.set_redirect("/index.html"); });
    } else {
        SPDLOG_WARN("Static UI not found. Expected orchestrator/src/interfaces/web/static relative to project root.");
    }

    m_server.Get("/status", [](const httplib::Request&, httplib::Response& response) {
        json json;
        json["project"] = "Chaos Engine";
        json["status"] = "Online (Web Adapter)";

        response.set_content(json.dump(4), "application/json");
    });

    m_server.Get("/containers", [this](const httplib::Request&, httplib::Response& response) {
        try {
            SPDLOG_DEBUG("/containers requested");
            auto containers = m_engine->listContainers();
            json json = json::array();
            for (const auto& container : containers) {
                json.push_back({{"id", container.id}, {"name", container.name}, {"state", container.state}});
            }
            response.set_content(json.dump(4), "application/json");
            SPDLOG_INFO("/containers served: {} items", containers.size());
        } catch (const std::exception& ex) {
            json error;
            error["error"] = "Failed to list containers";
            response.status = 500;
            response.set_content(error.dump(4), "application/json");
            SPDLOG_ERROR("/containers failed: {}", ex.what());
        }
    });

    m_server.Post(R"(/containers/([^/]+)/stop)", [this](const httplib::Request& request, httplib::Response& response) {
        if (request.matches.size() < 2) {
            response.status = 400;
            response.set_content(R"({"error":"Missing container id"})", "application/json");
            return;
        }

        const std::string containerId = request.matches[1];
        try {
            SPDLOG_INFO("/containers/{}/stop requested", containerId);
            m_engine->stopContainer(containerId);
            json json;
            json["status"] = "ok";
            json["action"] = "stop";
            json["id"] = containerId;
            response.set_content(json.dump(4), "application/json");
        } catch (const std::exception& ex) {
            json error;
            error["error"] = "Failed to stop container";
            response.status = 500;
            response.set_content(error.dump(4), "application/json");
            SPDLOG_ERROR("/containers/{}/stop failed: {}", containerId, ex.what());
        }
    });

    m_server.Post(R"(/containers/([^/]+)/kill)", [this](const httplib::Request& request, httplib::Response& response) {
        if (request.matches.size() < 2) {
            response.status = 400;
            response.set_content(R"({"error":"Missing container id"})", "application/json");
            return;
        }

        const std::string containerId = request.matches[1];
        try {
            SPDLOG_INFO("/containers/{}/kill requested", containerId);
            m_engine->killContainer(containerId);
            json json;
            json["status"] = "ok";
            json["action"] = "kill";
            json["id"] = containerId;
            response.set_content(json.dump(4), "application/json");
        } catch (const std::exception& ex) {
            json error;
            error["error"] = "Failed to kill container";
            response.status = 500;
            response.set_content(error.dump(4), "application/json");
            SPDLOG_ERROR("/containers/{}/kill failed: {}", containerId, ex.what());
        }
    });

    m_server.Get(R"(/containers/([^/]+)/logs)", [this](const httplib::Request& request, httplib::Response& response) {
        if (request.matches.size() < 2) {
            response.status = 400;
            response.set_content("Missing container id", "text/plain");
            return;
        }
        const std::string id = request.matches[1];
        try {
            const auto logs = m_engine->getLogs(id);
            response.set_content(logs, "text/plain");
        } catch (const std::exception& ex) {
            response.status = 500;
            json error;
            error["error"] = "Failed to get logs";
            response.set_content(error.dump(4), "application/json");
            SPDLOG_ERROR("/containers/{}/logs failed: {}", id, ex.what());
        }
    });

    // --- New API endpoints ---

    m_server.Get("/api/targets", [this](const httplib::Request& request, httplib::Response& response) {
        handleTargets(request, response);
    });

    m_server.Get("/api/limits", [this](const httplib::Request& request, httplib::Response& response) {
        handleLimits(request, response);
    });

    m_server.Post("/api/run", [this](const httplib::Request& request, httplib::Response& response) {
        handleRun(request, response);
    });

    m_server.Post("/api/run/abort", [this](const httplib::Request& request, httplib::Response& response) {
        handleAbort(request, response);
    });

    m_server.Get("/events", [this](const httplib::Request& request, httplib::Response& response) {
        handleEvents(request, response);
    });
}

void Server::handleTargets(const httplib::Request&, httplib::Response& response) {
    try {
        auto containers = m_engine->listContainers();
        json json = json::array();
        for (const auto& container : containers) {
            json.push_back({{"id", container.id}, {"name", container.name}, {"state", container.state}});
        }
        response.set_content(json.dump(4), "application/json");
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
        auto info = m_engine->getSystemInfo();
        json json = limitsToJson(info);
        response.set_content(json.dump(4), "application/json");
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

        auto session = std::make_shared<RunSession>();
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            m_session = session;
            session->running = true;
        }

        std::thread([engine = m_engine, session, manifest = std::move(manifest)]() {
            try {
                using namespace std::chrono;
                using namespace std::chrono_literals;

                perturbations::PerturbationFactory factory;
                std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbation_instances;
                perturbation_instances.reserve(manifest.perturbations.size());
                for (const auto& spec : manifest.perturbations) {
                    perturbation_instances.push_back(factory.create(engine, manifest.target, spec));
                }

                observability::ObservabilityEngine obs(engine);
                shared::TargetState lastKnownState;

                // Fetch IP once at setup.
                auto ip = obs.getContainerIp(manifest.target.id);
                if (ip) {
                    lastKnownState.container_ip = ip;
                }

                // Phase manager — runs on the calling thread.
                {
                    std::lock_guard<std::mutex> lock(session->mtx);
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
                            std::lock_guard<std::mutex> lock(session->mtx);
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
                            std::lock_guard<std::mutex> lock(session->mtx);
                            if (session->latest.has_value()) {
                                session->latest->recent_logs = std::move(logLines);
                            }
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
                    pert_engine.scheduleAllAsync(std::move(perturbation_instances), duration);
                    SPDLOG_INFO("Injecting faults for {}s.", duration.count());
                    {
                        std::lock_guard<std::mutex> lock(session->mtx);
                        session->phase = "chaos";
                    }
                    session->cv.notify_all();

                    // Wait for remaining chaos time + 2s recovery window.
                    std::this_thread::sleep_for(duration + 2s);
                }

                // Teardown and recovery.
                pert_engine.waitForTeardown();
                {
                    std::lock_guard<std::mutex> lock(session->mtx);
                    session->latest = lastKnownState;
                    session->phase = "recovery";
                }
                session->cv.notify_all();

                // Validation needs an authoritative final snapshot.
                lastKnownState = obs.observe(manifest.target.id);
                if (lastKnownState.container_ip) {
                    lastKnownState.container_ip = ip;
                }

                const auto results = validation::validate(lastKnownState, manifest.expectations);

                bool passed = std::ranges::all_of(results, [](const auto& result) { return result.passed; });
                json json;
                json["passed"] = passed;
                json["results"] = json::array();
                for (const auto& result : results) {
                    json["results"].push_back(
                        {{"type", result.expectationType}, {"passed", result.passed}, {"message", result.message}});
                }

                {
                    std::lock_guard<std::mutex> lock(session->mtx);
                    if (!session->running) {
                        return;
                    }
                    session->results = json;
                    session->running = false;
                    session->complete = true;
                }
                session->cv.notify_all();

                // jthread destructors call request_stop() + join() here.
            } catch (const std::exception& ex) {
                {
                    std::lock_guard<std::mutex> lock(session->mtx);
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
        std::lock_guard<std::mutex> lock(session_mutex_);
        session = m_session;
    }

    response.set_chunked_content_provider(
        "text/event-stream", [session](size_t /*offset*/, httplib::DataSink& sink) -> bool {
            if (!session) {
                if (!sink.write("event: error\ndata: {\"error\":\"No active run\"}\n\n", 47)) {
                    return false;
                }
                sink.done();
                return false;
            }

            std::unique_lock<std::mutex> lock(session->mtx);
            session->cv.wait_for(lock, std::chrono::milliseconds(100),
                                 [session]() { return session->latest.has_value() || session->complete; });

            if (session->latest.has_value()) {
                auto json = stateToJson(*session->latest, session->phase);
                std::string data = "event: state\ndata: " + json.dump() + "\n\n";
                if (!sink.write(data.data(), data.size())) {
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
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (!m_session || !m_session->running) {
            json json = {{"error", "No active run to abort"}};
            response.status = 409;
            response.set_content(json.dump(4), "application/json");
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
    json json = {{"status", "aborted"}};
    response.set_content(json.dump(4), "application/json");
}

}  // namespace chaos::orchestrator::interfaces::web
