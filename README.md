# CHAOS: Chaos Handling And Observability System

CHAOS is a C++23 resilience validation tool for containerized workloads.

Unlike production cluster chaos orchestrators, CHAOS focuses on development-time validation of a target workload under controlled stress. The project is optimized for deterministic local runs and clear diagnostics.

## Technical Objectives

- Fault isolation under constrained resources (CPU, RAM, network).
- Recovery validation under injected degradation.
- Real-time behavior visibility for the target process.

## Core Features

### Perturbations
CHAOS supports injecting various faults into target containers:
- **Resource Limits**: `cpu_cap`, `memory_cap` (Powered by Docker Update API)
- **Network Faults**: `network_delay`, `network_cutoff`, `packet_flood`, `traffic_corruption`
- **Lifecycle**: `kill`

### Observability & Validation
The `ObservationLoop` actively polls the Docker container via `ObservabilityEngine` to fetch real-time metrics (CPU, memory, network I/O) and logs at configurable intervals, pushing results to `SharedState` and notifying `IRunObserver`.
The `validation::validate` namespace-level free function evaluates JSON manifests automatically against these expectations:
- `container_running` / `container_not_running`
- `log_contains` / `log_not_contains`
- `http_status` / `http_latency`

### Concurrent Perturbations
Perturbations execute asynchronously using `std::jthread` with `std::stop_token` for concurrent fault injection. The `PerturbationEngine` manages their lifecycle, scheduling apply/revert cycles and providing thread-safe cancellation with built-in RAII join on destruction.

### Real-Time State Streaming
During perturbation runs, the system polls target state and broadcasts updates via `IRunObserver` (Web: `WebRunObserver` → `RunSession` → SSE; CLI: `CliRunObserver` → `StateBroadcaster`). The Web UI receives live updates via **SSE** (`GET /events`).

### Graceful Interruption
SIGINT (Ctrl+C) triggers immediate cancellation of running perturbations, with automatic rollback/reversion of all applied faults before the tool exits.

### Web UI (3-Step Flow)
A dark-themed SPA (`chaos serve`) with 3 auto-advancing steps:
- **Step 1: Configure** — compact form with target selector, duration, perturbation rows (6 types with type-aware param inputs), expectation rows, theme switcher (Amber/Dark/Cyber)
- **Step 2: Monitor** — real-time line charts (CPU, Memory, Network I/O) updated via SSE, container info panel, scrollable logs, **Abort** button to stop the run
- **Step 3: Results** — summary stats, timeline charts with Normal/Chaos/Recovery color zones, expectation validation results, run replay logs, **Run Again** and **Modify Manifest** buttons

### Shared Business Logic Layer
CHAOS centralises duplicated orchestration logic via `core::ChaosRunner`:
- **`buildPerturbations`** — creates concrete `IPerturbation` instances from manifest spec entries via `PerturbationFactory`.
    - **`finalize`** — evaluates container state against manifest expectations using `validation::validate()`. Receives continuous-failure data collected by `ObservationLoop` background threads every ~500ms during the run (failures tracked in `SharedState` but the run continues). Final validation runs on the state captured during chaos (before perturbations are reverted).
- **`parseManifest`** — reads and parses JSON manifest files via `ManifestParser`.

Both the CLI parser (`CliParser`) and Web Server (`Server`) are independent entry points that delegate to `RunOrchestrator` (lifecycle: normal → chaos → recovery → validate) and `ObservationLoop` (background metrics/logs collection), which in turn use `ChaosRunner` for validation and perturbation construction. `main.cpp` routes `serve` to the Server and all other commands to `CliParser`; neither adapter depends on the other. `SharedState` bridges observations to final validation. Tested with 15 unit tests and 1 E2E test.

### Signal Handling
The `signals::SignalHandlerGuard` RAII class (extracted from `CliParser.cpp`) manages POSIX signal handlers via a self-pipe trick. SIGINT writes to the pipe; the event loop reads from `readEnd()` and calls `request_stop()`. Tested with 5 unit tests.

### JSON Serialization
The `web::JsonSerializer` module (extracted from `Server.cpp`) provides `stateToJson`, `limitsToJson`, and `parseLogLines` helpers. These convert internal data types to JSON for SSE streaming. Tested with 7 unit tests.

### OpenTelemetry Export
CHAOS can export run events to OTLP-compatible backends (Grafana, Datadog, etc.) via the `OtlpExporter`, which sends JSON-encoded OTLP Logs over HTTP. Configure via `CHAOS_OTLP_ENDPOINT` environment variable.

## Architecture Overview

> **Deep Dive:** For a comprehensive breakdown of the system's design decisions, concurrency models and the Docker API Facade pattern, please read the [CHAOS Architecture Document](docs/ARCHITECTURE.md).

CHAOS uses a hybrid architecture:

- Top-level separation by deployable component:
    - `orchestrator/`: host binary (CLI/Web) and orchestration logic.
    - `frontend/`: Vite + pnpm project for the Web UI (builds to `orchestrator/src/interfaces/web/static/`).
    - `wrapper/`: in-container agent (`PID 1`) for process supervision and telemetry handoff.
- Inside `orchestrator/`, public contracts live in `include/` and implementation details stay in `src/`.
- Ownership is organized by responsibility (interfaces, containers, manifests, perturbations, observability, core, signals, web).

## Repository Layout

```text
chaos/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── frontend/                   # Vite + pnpm Web UI project
│   ├── index.html
│   ├── package.json
│   ├── vite.config.js
│   ├── vitest.config.js
│   └── src/
│       ├── main.jsx
│       ├── style.css
│       ├── components/
│       ├── constants/
│       │   └── __tests__/
│       ├── hooks/
│       ├── steps/
│       └── utils/
│           └── __tests__/
├── orchestrator/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── containers/
│   │   ├── core/
│   │   ├── manifests/
│   │   ├── observability/
│   │   ├── perturbations/
│   │   ├── shared/
│   │   ├── signals/
│   │   ├── validation/
│   │   └── web/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── core/
│   │   ├── interfaces/
│   │   │   ├── cli/
│   │   │   └── web/
│   │   ├── containers/
│   │   │   └── internal/
│   │   ├── manifests/
│   │   ├── perturbations/
│   │   │   └── internal/
│   │   ├── observability/
│   │   │   └── internal/
│   │   ├── signals/
│   │   ├── validation/
│   │   │   └── internal/
│   └── tests/
│       ├── containers/
│       │   └── internal/
│       ├── core/
│       ├── interfaces/
│       │   └── cli/
│       ├── manifests/
│       ├── observability/
│       ├── perturbations/
│       ├── signals/
│       ├── smoke/
│       └── web/
├── wrapper/
│   ├── CMakeLists.txt
│   ├── src/
│   └── tests/
├── tests/
│   └── e2e/
└── docs/
```

## Standard Build & Test Commands

For direct CMake control, use these exact commands:

- Build the C++ orchestrator: `cmake --build --preset debug-clang -- -j$(nproc)`
- Build the Web UI (required before C++ build, or whenever frontend changes): `cd frontend && pnpm install && pnpm run build`
- Full build (frontend + C++): `cd frontend && pnpm install && pnpm run build && cd .. && cmake --build --preset debug-clang -- -j$(nproc)`
- C++ test: `ctest --test-dir build/dev-linux-clang --output-on-failure`
- Frontend test: `cd frontend && pnpm test` (uses Vitest, 20+ tests for hooks, utils, constants)
- Build the Web UI: see [Build the Web UI](#build-the-web-ui) below

## Privilege Requirements

Network perturbations (`network_delay`, `network_cutoff`, `packet_flood`, `traffic_corruption`) use `nsenter` or `setns()` to inject tc/iptables rules or raw packets directly into the target container's network namespace. This requires **root privileges** — run the orchestrator with `sudo` or as root.

## Run the Orchestrator (CLI)

After building:

- `sudo ./build/dev-linux-clang/orchestrator/chaos help`
- `sudo ./build/dev-linux-clang/orchestrator/chaos list`
- `sudo ./build/dev-linux-clang/orchestrator/chaos stop <container_id>`
- `sudo ./build/dev-linux-clang/orchestrator/chaos kill <container_id>`

## Build the Web UI

The Web UI is a Vite + pnpm project in `frontend/`:

```bash
cd frontend
pnpm install
pnpm run build    # outputs to orchestrator/src/interfaces/web/static/
```

During development, use `pnpm run dev` for hot module reload at `http://localhost:5173`.

## Run the Web UI

Start the server:

- `sudo ./build/dev-linux-clang/orchestrator/chaos serve --port 8080`

Then open:

- `http://127.0.0.1:8080`

The Web UI has a 3-step flow:

1. **Configure** — select target, set duration, add perturbations with parameters, configure expectations, click "Run Chaos Test"
2. **Monitor** — watch live Recharts line charts (CPU, Memory, Network I/O), container info, and logs update in real time via SSE. Click **Abort** to stop
3. **Results** — see summary stats, timeline charts with Normal/Chaos/Recovery color zones, expectation results, and run logs. Click **Run Again** or **Modify Manifest**

Switch themes (Amber / Dark / Cyber) from the topbar dropdown. Your preference is saved to localStorage.

### API Endpoints

| Method | Route | Description |
|--------|-------|-------------|
| GET | `/api/targets` | List available containers `[{id, name, state}]` |
| GET | `/api/limits` | System limits `{cpu_cores, memory_total_mb, perturbation_limits}` |
| POST | `/api/run` | Async run — returns 202, streams state via SSE |
| POST | `/api/run/abort` | Abort the currently running test |
| GET | `/events` | SSE stream (`event: state` / `event: complete` / `event: error`) |

| POST | `/containers/{id}/stop` | Stop a container |
| POST | `/containers/{id}/kill` | Kill a container |
| GET | `/containers/{id}/logs` | Fetch container logs |

## Testing Strategy

- `orchestrator/tests/containers/internal`: module unit tests (Docker adapter ownership).
- `orchestrator/tests/interfaces/cli`: CLI parser unit tests.
- `orchestrator/tests/manifests`: manifest parsing unit tests.
- `orchestrator/tests/observability`: observability, validation engine, and OTLP exporter unit tests.
- `orchestrator/tests/shared`: StateBroadcaster unit tests (subscribe/unsubscribe, concurrency, exception safety).
- `orchestrator/tests/perturbations`: perturbation engine and factory unit tests.
- `orchestrator/tests/core`: `ChaosRunner` unit tests (build perturbations, validate expectations, parse manifest).
- `orchestrator/tests/signals`: `SignalHandlerGuard` unit tests (RAII, pipe, self-signal).
- `orchestrator/tests/web`: `JsonSerializer` unit tests (state/limits JSON, log parsing).
- `orchestrator/tests/smoke`: minimal host smoke checks through public APIs (no direct `internal` includes).
- `frontend/src/utils/__tests__/` and `frontend/src/constants/__tests__/`: Vitest unit tests for frontend utilities and constants.
- `tests/e2e`: cross-component integration tests.

## Project Roadmap

CHAOS is under active development to expand its SRE capabilities, moving towards advanced network fault injection, in-container PID 1 orchestration, and modern C++23 concurrency models. 

> **Vision & Next Steps:** To explore our planned features, architectural evolutions, and upcoming technical milestones, please review the [CHAOS Project Roadmap](ROADMAP.md).
