#pragma once

namespace chaos::domain::ports {

/**
 * @brief Port for user-facing adapters (driving/primary side).
 *
 * Any UI adapter (CLI, Web server, TUI, etc.) implements this port
 * so the application entry point remains decoupled from the concrete
 * interaction mechanism.
 */
class IUserInterface {
public:
    virtual ~IUserInterface() = default;

    /**
     * @brief Start the user interface.
     * @param argc Argument count forwarded from main().
     * @param argv Argument values forwarded from main().
     * @return Exit code (0 on success, non-zero on error).
     */
    virtual int run(int argc, char* argv[]) = 0;
};

}  // namespace chaos::domain::ports
