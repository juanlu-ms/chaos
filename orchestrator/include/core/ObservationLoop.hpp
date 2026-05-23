#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "containers/IContainerEngine.hpp"
#include "core/IRunObserver.hpp"
#include "core/SharedState.hpp"
#include "manifests/Manifest.hpp"

namespace chaos::orchestrator::containers::internal {
class CgroupMetricsGatherer;
}

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
    struct Config {
        std::chrono::milliseconds metricsInterval{100ms};
        std::chrono::milliseconds logsInterval{1500ms};
        /**
         * @brief Interval for continuous-expectation validation.
         *        Default 500ms matches the current hardcoded behaviour
         *        (every ~5th tick at 100ms metrics interval).
         */
        std::chrono::milliseconds continuousValidationInterval{500ms};
        /**
         * @brief Expectations whose `continuous` flag is true are
         *        validated at each continuousValidationInterval tick
         *        and failures are recorded via
         *        SharedState::addContinuousFailure().
         */
        std::vector<manifests::Expectation> continuousExpectations;
    };

    ObservationLoop(std::shared_ptr<containers::IContainerEngine> engine,
                    std::string containerId,
                    SharedState& state,
                    IRunObserver& observer,
                    Config config = {});

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
    void metricsThreadFn(std::stop_token internal_stop,
                         std::stop_token external_stop);
    void logsThreadFn(std::stop_token internal_stop,
                      std::stop_token external_stop);

    std::shared_ptr<containers::IContainerEngine> engine_;
    std::string containerId_;
    SharedState& state_;
    IRunObserver& observer_;
    Config config_;

    std::optional<containers::internal::CgroupMetricsGatherer> cgroup_;
    bool useCgroup_{false};

    std::optional<std::string> containerIp_;

    std::stop_source internalStopSource_;
    std::jthread metricsThread_;
    std::jthread logsThread_;
};

}  // namespace chaos::orchestrator::core
