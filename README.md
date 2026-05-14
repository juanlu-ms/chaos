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
- **Network Faults**: `network_delay`, `network_cutoff`, `garbage_packet`
- **Lifecycle**: `kill`

### Observability & Validation
The `ObservabilityEngine` provides real-time container state inspection via `observe()`, which returns a `TargetState` snapshot including status, logs, and resource metrics.
The `ValidationEngine` evaluates JSON manifests automatically against these expectations:
- `container_running` / `container_not_running`
- `log_contains` / `log_not_contains`
- `http_status` / `http_latency`

### Concurrent Perturbations
Perturbations execute asynchronously using `std::async` for concurrent fault injection. The `PerturbationEngine` manages their lifecycle, scheduling apply/revert cycles and providing thread-safe cancellation.

### Real-Time State Streaming
During perturbation runs, the system polls target state and broadcasts updates via `StateBroadcaster`. The Web UI receives live updates via **SSE** (`GET /events`). `StateBroadcaster` supports exception-safe handle-based subscribe/unsubscribe.

### Graceful Interruption
SIGINT (Ctrl+C) triggers immediate cancellation of running perturbations, with automatic rollback/reversion of all applied faults before the tool exits.

### Web UI (3-Step Flow)
A dark-themed SPA (`chaos serve`) with 3 auto-advancing steps:
- **Step 1: Configure** — compact form with target selector, duration, perturbation rows (6 types with type-aware param inputs), expectation rows, theme switcher (Amber/Dark/Cyber)
- **Step 2: Monitor** — real-time line charts (CPU, Memory, Network I/O) updated via SSE, container info panel, scrollable logs, **Abort** button to stop the run
- **Step 3: Results** — summary stats, timeline charts with Normal/Chaos/Recovery color zones, expectation validation results, run replay logs, **Run Again** and **Modify Manifest** buttons

### OpenTelemetry Export
CHAOS can export run events to OTLP-compatible backends (Grafana, Datadog, etc.) via the `OtlpExporter`, which sends JSON-encoded OTLP Logs over HTTP. Configure via `CHAOS_OTLP_ENDPOINT` environment variable.

## Architecture Overview

> **Deep Dive:** For a comprehensive breakdown of the system's design decisions, concurrency models and the Docker API Facade pattern, please read the [CHAOS Architecture Document](docs/ARCHITECTURE.md).

CHAOS uses a hybrid architecture:

- Top-level separation by deployable component:
    - `orchestrator/`: host binary (CLI/Web) and orchestration logic.
    - `frontend/`: Vite + Bun project for the Web UI (builds to `orchestrator/src/interfaces/web/static/`).
    - `wrapper/`: in-container agent (`PID 1`) for process supervision and telemetry handoff.
- Inside `orchestrator/`, public contracts live in `include/` and implementation details stay in `src/`.
- Ownership is organized by responsibility (interfaces, containers, manifests, perturbations, observability).

## Repository Layout

```text
chaos/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── frontend/                   # Vite + Bun Web UI project
│   ├── index.html
│   ├── package.json
│   ├── vite.config.js
│   └── src/
│       ├── main.jsx
│       └── style.css
├── orchestrator/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── containers/
│   │   ├── manifests/
│   │   ├── observability/
│   │   ├── perturbations/
│   │   ├── shared/
│   │   └── validation/
│   ├── src/
│   │   ├── main.cpp
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
│   │   ├── validation/
│   │   │   └── internal/
│   └── tests/
│       ├── containers/
│       │   └── internal/
│       ├── interfaces/
│       │   └── cli/
│       ├── manifests/
│       ├── observability/
│       ├── perturbations/
│       └── smoke/
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
- Build the Web UI (required before C++ build, or whenever frontend changes): `cd frontend && bun install && bun run build`
- Full build (frontend + C++): `cd frontend && bun install && bun run build && cd .. && cmake --build --preset debug-clang -- -j$(nproc)`
- Test: `ctest --test-dir build/dev-linux-clang --output-on-failure`
- Build the Web UI: see [Build the Web UI](#build-the-web-ui) below

## Run the Orchestrator (CLI)

After building:

- `./build/dev-linux-clang/orchestrator/chaos help`
- `./build/dev-linux-clang/orchestrator/chaos list`
- `./build/dev-linux-clang/orchestrator/chaos stop <container_id>`
- `./build/dev-linux-clang/orchestrator/chaos kill <container_id>`

## Build the Web UI

The Web UI is a Vite + Bun project in `frontend/`:

```bash
cd frontend
bun install
bun run build    # outputs to orchestrator/src/interfaces/web/static/
```

During development, use `bun run dev` for hot module reload at `http://localhost:5173`.

## Run the Web UI

Start the server:

- `./build/dev-linux-clang/orchestrator/chaos serve --port 8080`

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
| POST | `/run` | Legacy synchronous run (backward compatible) |
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
- `orchestrator/tests/smoke`: minimal host smoke checks through public APIs (no direct `internal` includes).
- `tests/e2e`: cross-component integration tests.

## Project Roadmap

CHAOS is under active development to expand its SRE capabilities, moving towards advanced network fault injection, in-container PID 1 orchestration, and modern C++23 concurrency models. 

> **Vision & Next Steps:** To explore our planned features, architectural evolutions, and upcoming technical milestones, please review the [CHAOS Project Roadmap](ROADMAP.md).
