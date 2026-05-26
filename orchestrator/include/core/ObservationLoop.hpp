/**
 * @file ObservationLoop.hpp
 * @brief Background observation threads for metrics and logs collection.
 */

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "containers/IContainerEngine.hpp"
namespace chaos::orchestrator::containers {
class CgroupMetricsGatherer;
}
#include "core/IRunObserver.hpp"
#include "core/SharedState.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::core {

/**
 * @brief Runs background jthreads that observe metrics and logs at
 *        configurable intervals, pushing results to SharedState.
 *
 * Ownership: each instance owns its own std::stop_source. The destructor
 * requests stop and joins all threads — no external coordination needed.
 *
 * The metrics thread also performs continuous-expectation validation
 * at a configurable interval using the expectations list provided in Config.
 */
class ObservationLoop {
public:
    /**
     * @brief Configuration for observation intervals and continuous expectations.
     */
    struct Config {
        Config(std::chrono::milliseconds metricsInterval, std::chrono::milliseconds logsInterval,
               std::chrono::milliseconds continuousValidationInterval)
            : metricsInterval(metricsInterval),
              logsInterval(logsInterval),
              continuousValidationInterval(continuousValidationInterval) {}

        /** @brief Interval between metrics collection ticks. Default 100ms. */
        std::chrono::milliseconds metricsInterval{100};

        /** @brief Interval between log collection ticks. Default 1500ms. */
        std::chrono::milliseconds logsInterval{1500};

        /**
         * @brief Interval for continuous-expectation validation.
         *        Default 500ms matches the current hardcoded behaviour
         *        (every ~5th tick at 100ms metrics interval).
         */
        std::chrono::milliseconds continuousValidationInterval{500};

        /**
         * @brief Expectations whose `continuous` flag is true are
         *        validated at each continuousValidationInterval tick
         *        and failures are recorded via
         *        SharedState::addContinuousFailure().
         */
        std::vector<manifests::Expectation> continuousExpectations;
    };

    /**
     * @brief Construct an ObservationLoop.
     * @param engine Container engine for fetching metrics and logs.
     * @param containerId Target container identifier.
     * @param state SharedState to populate with observed data.
     * @param observer Observer notified on each update.
     * @param config Interval and expectation configuration.
     */
    ObservationLoop(std::shared_ptr<containers::IContainerEngine> engine, std::string containerId, SharedState& state,
                    IRunObserver& observer, Config config);

    ~ObservationLoop();

    ObservationLoop(const ObservationLoop&) = delete;
    ObservationLoop& operator=(const ObservationLoop&) = delete;
    ObservationLoop(ObservationLoop&&) = delete;
    ObservationLoop& operator=(ObservationLoop&&) = delete;

    /**
     * @brief Start the background threads. Call at most once.
     * @param external_stop Optional external cancellation token (e.g.
     *                      signal guard or session abort).
     */
    void start(std::stop_token external_stop = {});

private:
    // Background loop for metrics collection and continuous validation.
    void metricsThreadFn(std::stop_token internal_stop, std::stop_token external_stop);

    // Background loop for log collection.
    void logsThreadFn(std::stop_token internal_stop, std::stop_token external_stop);

    // Background loop for network latency measurement (ICMP ping).
    void latencyThreadFn(std::stop_token internal_stop, std::stop_token external_stop);

    std::shared_ptr<containers::IContainerEngine> engine_;
    std::string containerId_;
    SharedState& state_;
    IRunObserver& observer_;
    Config config_;

    // Cgroup-based metrics gathering (optional, resolved at start time).
    std::unique_ptr<containers::CgroupMetricsGatherer> cgroup_;

    // Container IP string, fetched once at start and stamped on every state.
    // Set once before threads launch, then read-only — no synchronization needed.
    std::optional<std::string> containerIp_;

    // Host PID of the container's init process, fetched once at start.
    // Used to read /proc/<pid>/net/dev for fast network stats.
    int containerPid_{0};

    std::stop_source internalStopSource_;
    std::jthread metricsThread_;
    std::jthread logsThread_;
    std::jthread latencyThread_;
};

}  // namespace chaos::orchestrator::core
