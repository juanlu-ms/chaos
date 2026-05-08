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
During perturbation runs, the system polls target state and broadcasts updates via `StateBroadcaster`. The Web UI receives live updates via **SSE** (`GET /events`), and the TUI subscribes directly. `StateBroadcaster` supports exception-safe handle-based subscribe/unsubscribe.

### Graceful Interruption
SIGINT (Ctrl+C) triggers immediate cancellation of running perturbations, with automatic rollback/reversion of all applied faults before the tool exits.

### Web Dashboard (Manifest Builder + Live Monitor)
A dark-themed SPA (`chaos serve`) featuring:
- **Dashboard view** — target container cards with live system limits
- **Manifest Builder** — form-based scenario creation: pick target, add/remove perturbations (6 types with type-aware parameter inputs), configure expectations, set duration
- **Live Monitor** — real-time SSE state updates during runs (CPU, memory, status), results panel with pass/fail validation

### TUI (Dashboard + Wizard)
Two-mode interactive terminal UI (`chaos tui`):
- **Dashboard** — container list with arrow navigation, live target state panel (status, CPU, memory)
- **Wizard** — 4-step guided manifest creation (select target → add perturbations → set expectations → run with progress bar)

### OpenTelemetry Export
CHAOS can export run events to OTLP-compatible backends (Grafana, Datadog, etc.) via the `OtlpExporter`, which sends JSON-encoded OTLP Logs over HTTP. Configure via `CHAOS_OTLP_ENDPOINT` environment variable.

## Architecture Overview

> **Deep Dive:** For a comprehensive breakdown of the system's design decisions, concurrency models and the Docker API Facade pattern, please read the [CHAOS Architecture Document](docs/ARCHITECTURE.md).

CHAOS uses a hybrid architecture:

- Top-level separation by deployable component:
    - `orchestrator/`: host binary (CLI/Web) and orchestration logic.
    - `wrapper/`: in-container agent (`PID 1`) for process supervision and telemetry handoff.
- Inside `orchestrator/`, public contracts live in `include/` and implementation details stay in `src/`.
- Ownership is organized by responsibility (interfaces, containers, manifests, perturbations, observability).

## Repository Layout

```text
chaos/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
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
│   │   │   ├── tui/
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

- Build: `cmake --build --preset debug-clang -- -j$(nproc)`
- Test: `ctest --test-dir build/dev-linux-clang --output-on-failure`

## Run the Orchestrator (CLI)

After building:

- `./build/dev-linux-clang/orchestrator/chaos help`
- `./build/dev-linux-clang/orchestrator/chaos list`
- `./build/dev-linux-clang/orchestrator/chaos stop <container_id>`
- `./build/dev-linux-clang/orchestrator/chaos kill <container_id>`

## Run the Web UI

Start the server:

- `./build/dev-linux-clang/orchestrator/chaos serve --port 8080`

Then open:

- `http://127.0.0.1:8080`

The Web UI has two tabs (Dashboard / Manifest Builder). To run a scenario:

1. Select a target container
2. Add perturbations (kill, cpu_cap, memory_cap, network_delay, etc.) with parameters
3. Set expectations (container_running, log_contains, http_status, etc.)
4. Click "Run Scenario" — monitor panel shows live state updates via SSE

### API Endpoints

| Method | Route | Description |
|--------|-------|-------------|
| GET | `/api/targets` | List available containers `[{id, name, state}]` |
| GET | `/api/limits` | System limits `{cpu_cores, memory_total_mb, perturbation_limits}` |
| POST | `/api/run` | Async run — returns 202, streams state via SSE |
| GET | `/events` | SSE stream (`event: state` / `event: complete`) |
| POST | `/run` | Legacy synchronous run (backward compatible) |
| POST | `/containers/{id}/stop` | Stop a container |
| POST | `/containers/{id}/kill` | Kill a container |
| GET | `/containers/{id}/logs` | Fetch container logs |

## Run the TUI

Launch the interactive terminal UI:

- `./build/dev-linux-clang/orchestrator/chaos tui`

Tab toggles between Dashboard mode (container list + live state) and Wizard mode (4-step manifest builder). q/Escape to go back or quit.

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
