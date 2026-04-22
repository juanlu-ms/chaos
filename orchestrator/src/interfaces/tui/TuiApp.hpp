/**
 * @file TuiApp.hpp
 * @brief Terminal User Interface for the chaos orchestrator.
 */
#pragma once

#include <memory>

#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::interfaces::tui {

/**
 * @brief Launches and runs the interactive TUI dashboard.
 *
 * Provides an interactive, full-screen terminal UI with container
 * listing, stop/kill actions, and manifest execution.
 * Blocks until the user exits (presses 'q' or ESC).
 */
class TuiApp {
public:
    /**
     * @brief Construct a new TuiApp.
     * @param engine Container engine instance for Docker operations.
     */
    explicit TuiApp(std::shared_ptr<containers::IContainerEngine> engine);

    /**
     * @brief Runs the interactive TUI. Blocks until user exits.
     * @return 0 on clean exit.
     */
    int run() const;

private:
    std::shared_ptr<containers::IContainerEngine> m_engine;
};

}  // namespace chaos::orchestrator::interfaces::tui
