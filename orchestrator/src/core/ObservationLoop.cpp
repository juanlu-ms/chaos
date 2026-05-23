#include "core/ObservationLoop.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <thread>

#include "containers/internal/CgroupMetricsGatherer.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::core {

using namespace std::chrono_literals;

ObservationLoop::ObservationLoop(std::shared_ptr<containers::IContainerEngine> engine, std::string containerId,
                                 SharedState& state, IRunObserver& observer, Config config)
    : engine_(std::move(engine)),
      containerId_(std::move(containerId)),
      state_(state),
      observer_(observer),
      config_(std::move(config)) {}

ObservationLoop::~ObservationLoop() { internalStopSource_.request_stop(); }

void ObservationLoop::start(std::stop_token external_stop) {
    if (containers::internal::CgroupMetricsGatherer::resolveCgroupPath(containerId_).has_value()) {
        useCgroup_ = true;
        SPDLOG_DEBUG("ObservationLoop: using cgroup v2 for {}", containerId_);
    }

    observability::ObservabilityEngine initObs(engine_);
    containerIp_ = initObs.getContainerIp(containerId_);

    auto internal_stop = internalStopSource_.get_token();

    metricsThread_ =
        std::jthread([this, internal_stop, external_stop] { metricsThreadFn(internal_stop, external_stop); });

    logsThread_ = std::jthread([this, internal_stop, external_stop] { logsThreadFn(internal_stop, external_stop); });
}

void ObservationLoop::metricsThreadFn(std::stop_token internal_stop, std::stop_token external_stop) {
    observability::ObservabilityEngine obs(engine_);
    shared::TargetState state;
    auto lastContinuousCheck = std::chrono::steady_clock::now();

    while (!internal_stop.stop_requested() && !external_stop.stop_requested()) {
        auto tick = std::chrono::steady_clock::now();

        state = obs.observe(containerId_);
        if (containerIp_) state.container_ip = *containerIp_;
        state_.updateState(state);
        observer_.onStateUpdate(state);

        auto timeSinceLastCheck = tick - lastContinuousCheck;
        if (timeSinceLastCheck >= config_.continuousValidationInterval && !config_.continuousExpectations.empty()) {
            lastContinuousCheck = tick;

            std::vector<manifests::Expectation> continuous;
            std::copy_if(config_.continuousExpectations.begin(), config_.continuousExpectations.end(),
                         std::back_inserter(continuous), [](const auto& e) { return e.continuous; });

            if (!continuous.empty()) {
                auto results = validation::validate(state, continuous);
                for (const auto& r : results) {
                    if (!r.passed) {
                        state_.addContinuousFailure(r.expectationType);
                        SPDLOG_WARN("Continuous expectation '{}' failed", r.expectationType);
                    }
                }
            }
        }

        auto elapsed = std::chrono::steady_clock::now() - tick;
        auto remaining = config_.metricsInterval - elapsed;
        if (remaining > 0ms) {
            std::this_thread::sleep_for(remaining);
        }
    }
}

void ObservationLoop::logsThreadFn(std::stop_token internal_stop, std::stop_token external_stop) {
    observability::ObservabilityEngine obs(engine_);

    while (!internal_stop.stop_requested() && !external_stop.stop_requested()) {
        auto tick = std::chrono::steady_clock::now();

        auto rawLogs = obs.getLogs(containerId_);
        std::vector<std::string> logLines;
        observability::parseLogLines(rawLogs, logLines);
        state_.updateLogs(logLines);
        observer_.onLogsUpdate(logLines);

        auto elapsed = std::chrono::steady_clock::now() - tick;
        auto remaining = config_.logsInterval - elapsed;
        if (remaining > 0ms) {
            std::this_thread::sleep_for(remaining);
        }
    }
}

}  // namespace chaos::orchestrator::core
