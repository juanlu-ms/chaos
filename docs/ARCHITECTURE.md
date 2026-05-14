# Project Overview: CHAOS (Resilience Tool for Docker Containers)
CHAOS is an enterprise-grade Chaos Engineering orchestrator built in modern C++ (C++23). Its primary purpose is to inject controlled faults (perturbations) into running Docker containers to validate system resilience, test observability, and ensure that applications recover gracefully from infrastructure degradation (following Site Reliability Engineering principles). 

The tool supports multiple user interfaces—a Command Line Interface (CLI) and a Web Server—all powered by a highly concurrent, thread-safe, and event-driven core backend. The Web UI is a separate Vite + Bun project in `frontend/` using React with Recharts for real-time and results charts, built to static files that the C++ binary serves.

---

## Project Origins & Initial Goals (The PID 1 Wrapper)

The inception of the CHAOS project was rooted in testing internal application recovery mechanisms, specifically focusing on process lifecycle management inside the container. 

* **The PID 1 Wrapper:** Initially, a core goal was to evaluate how a system handles sudden, traumatic failures (like a SIGKILL or an Out-Of-Memory/OOM event). To achieve this, the project explored the use of a custom **PID 1 Wrapper** deployed inside the target containers. 
* **Purpose of the Wrapper:** In Linux, PID 1 is the `init` process, responsible for handling signals and reaping orphan child processes. The wrapper was designed to intercept operating system signals, manage the main application process, and attempt to safely recover or restart the application after a crash.
* **Data Resilience Testing:** The primary metric for success at this stage was assessing "in-flight" data resilience: verifying whether the wrapper and the system could prevent the loss of transactions or the corruption of files when the container's main process was abruptly killed or asphyxiated by memory limits. 
* **Evolution:** As the project matured, it expanded from internal process monitoring (the wrapper) into a broader, infrastructure-level orchestrator capable of stressing external container boundaries (cgroups) and validating the results automatically.

---

## Core Architecture & Components

The system is designed using SOLID principles, enforcing strict separation of concerns through various architectural patterns.

* **Manifest Parsing & Command Generation:**
    * **ManifestParser:** Reads JSON files containing the test configuration (target container, perturbation types, duration, and success expectations).
    * **PerturbationFactory:** Implements the **Factory Pattern**. It translates string identifiers from the JSON (e.g., `kill`, `memory_cap`, `cpu_cap`, `network_delay`) into concrete objects that implement the `IPerturbation` interface.
* **Execution Engine (Asynchronous Task Scheduler):**
    * **PerturbationEngine:** The heart of the orchestrator. It uses the **Command Pattern** to encapsulate faults. Instead of applying faults synchronously, it dispatches each perturbation to an isolated background thread using `std::jthread` (with `std::stop_token` for cancellation). This allows multiple faults to be injected simultaneously with independent lifecycles.
* **Orchestration Layer:**
    * **ChaosRunner (core module):** Sits between the adapters (CLI/Server) and the lower-level components. Provides three operations: `buildPerturbations` (delegates to `PerturbationFactory`), `validateExpectations` (delegates to `ValidationEngine`), and `parseManifest` (delegates to `ManifestParser`). Both `CliParser` and `Server` delegate to `ChaosRunner`, eliminating the business logic duplication that previously existed.
* **Real-Time Observability & Event Bus:**
    * **ObservabilityEngine:** Actively polls the Docker container to fetch real-time metrics during a chaos experiment.
    * **StateBroadcaster:** Implements the **Observer Pattern** acting as a thread-safe event bus. The engine publishes container states here, and consumers subscribe to receive real-time updates without data races.
* **Validation:** * **ValidationEngine:** Evaluates the container's final or continuous state against the expectations defined in the manifest (e.g., verifying if the container correctly logged an `OOMKilled` exit code 137).

---

## Infrastructure Interaction: The Docker Facade

A critical design decision in CHAOS is how it interacts with the underlying Linux kernel restrictions.

* **The Facade Pattern:** The system **does not** manipulate Linux `cgroups` (Control Groups) manually, avoiding technical debt and compatibility issues between cgroups v1 and v2.
* **API-First:** The `IContainerEngine` acts as a Facade, communicating exclusively with the Docker Daemon via its REST API using partial JSON payloads. 
* **Safe Teardown (Resetting Resources):** 
    * To remove a CPU cap, the system sends a `CpuQuota` of `-1` (the scheduler's default for unlimited).
    * To remove a Memory cap, the system dynamically queries the host's total physical RAM (via `GET /info`) and sets the container's limit to match the host hardware, effectively removing the restriction.

---

## Concurrency and Thread Safety Mechanisms

The backend is hardened for production, handling parallel execution and emergency aborts flawlessly.

* **Mutexes & Swap Pattern:** The `active_tasks_` registry is protected by `std::mutex`. During teardown, a "Swap Pattern" is used to copy tasks under a lock and execute the teardown outside the lock, preventing deadlocks.
* **Condition Variables (`std::condition_variable`):** Background threads use conditional waits. If an emergency abort is triggered, a `notify_all()` signal instantly wakes all threads, allowing them to revert their specific faults in milliseconds.
* **POSIX Signal Handling (SIGINT):** The CLI safely catches `CTRL+C` interrupts using `volatile std::sig_atomic_t` to guarantee hardware-level thread safety, and a `SignalHandlerGuard` (RAII) to ensure the terminal's default behavior is restored.
* **Exception Isolation:** During teardown, if reverting one fault fails, the system catches the exception, flags the test as having errors, and proceeds to clean up the remaining faults, preventing host corruption.

---

## Web UI (Frontend)

The Web UI lives in `frontend/` as a **Vite + Bun** project. It is built to static files in `orchestrator/src/interfaces/web/static/`, which the C++ binary serves via `httplib::Server::set_mount_point()`.

### Tech Stack
- **Vite** — fast dev server with HMR, production bundling
- **Bun** — package manager and runtime
- **React** — UI framework with component-based architecture
- **Recharts** — charting library for live and results charts

### Frontend Component Architecture
The UI is built around reusable, extracted modules:
- **Hooks** (`src/hooks/`): `useSSEStream` (SSE lifecycle, reconnection, epoch tracking), `useStreamData` (SSE event transformation into chart data/logs/phase zones), `useElapsedTimer` (live timer for Step2Monitor).
- **Components** (`src/components/`): `ChartBase` (shared Recharts config: axes, tooltip, gradient, theme), `ConfigRow` (generic type-aware row used by both perturbations and expectations).
- **Utilities** (`src/utils/`): `validateManifest` (form validation), `stringifyParams` (parameter serialization). Both fully unit tested.
- **Constants** (`src/constants/`): `perturbations.js` and `expectations.js` use a consistent dict-based pattern.
- **Test infrastructure:** Vitest with `globals: true`, 20+ unit tests across all modules.

### 3-Step Flow
1. **Configure** — form-based manifest builder (target, duration, perturbations, expectations)
2. **Monitor** — real-time Recharts line charts (CPU, Memory, Network I/O) fed by SSE `GET /events`
3. **Results** — timeline charts with Normal/Chaos/Recovery zone backgrounds via Recharts `ReferenceArea`

### Real-Time Data Flow
```
Step 1: POST /api/run  (manifest JSON) → returns 202
Step 2: EventSource /events  → SSE stream
  - event: state  → useSSEStream calls handleState → useStreamData transforms → Recharts AreaChart
  - event: complete → closeStream() → showResults() → Recharts AreaChart with full dataset
Callback: Abort → POST /api/run/abort → closeStream()
```

### Theming
Three themes (Amber, Dark, Cyber) defined as CSS custom properties in `src/style.css`. Theme selection persisted in `localStorage`. Recharts colors are re-read from CSS vars on theme change.

### Development
```bash
cd frontend && bun install && bun run dev     # HMR at localhost:5173
cd frontend && bun run build                     # output to static/
```

The built files are served by the C++ binary — no separate web server needed at runtime.
