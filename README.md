# CHAOS: Chaos Handling And Observability System

CHAOS is a C++23 resilience validation tool for containerized workloads.

Unlike production cluster chaos orchestrators, CHAOS focuses on development-time validation of a target workload under controlled stress. The project is optimized for deterministic local runs and clear diagnostics.

## Technical Objectives

- Fault isolation under constrained resources (CPU, RAM, network).
- Recovery validation under injected degradation.
- Real-time behavior visibility for the target process.

## Architecture Overview

CHAOS uses a hybrid architecture:

- Top-level separation by deployable component:
	- `orchestrator/`: host binary (CLI/Web) and orchestration logic.
	- `wrapper/`: in-container agent (`PID 1`) for process supervision and telemetry handoff.
	- `ebpf_src/`: kernel-side eBPF program sources.
- Inside `orchestrator/`, code is organized by responsibility and ownership (interfaces, containers, scenarios, perturbations, observability).

## Repository Layout

```text
chaos/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── orchestrator/
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp
│   │   ├── interfaces/
│   │   │   ├── cli/
│   │   │   └── web/
│   │   ├── containers/
│   │   │   └── internal/
│   │   ├── scenarios/
│   │   ├── perturbations/
│   │   │   └── internal/
│   │   ├── observability/
│   │   │   └── internal/
│   │   └── shared/
│   └── tests/
│       ├── containers/
│       │   └── internal/
│       └── smoke/
├── wrapper/
│   ├── CMakeLists.txt
│   ├── src/
│   └── tests/
├── ebpf_src/
│   ├── CMakeLists.txt
│   └── src/
├── tests/
│   └── e2e/
└── docs/
```

## Local Build (portable)

To avoid exporting `VCPKG_ROOT` manually, use:

- `bash scripts/cmake-local.sh all`

The script auto-detects `vcpkg` in this order:

1. `VCPKG_ROOT` (if already set and valid)
2. `./vcpkg`
3. `/vcpkg`

You can run a single stage too:

- `bash scripts/cmake-local.sh configure`
- `bash scripts/cmake-local.sh build`
- `bash scripts/cmake-local.sh test`

## Run the Orchestrator (CLI)

After building:

- `./build/debug/orchestrator/chaos help`
- `./build/debug/orchestrator/chaos list`
- `./build/debug/orchestrator/chaos stop <container_id>`
- `./build/debug/orchestrator/chaos kill <container_id>`

## Run the Web UI

Start the server:

- `./build/debug/orchestrator/chaos serve --port 8080`

Then open:

- `http://127.0.0.1:8080`

## Testing Strategy

- `orchestrator/tests/containers/internal`: module unit tests (Docker adapter ownership).
- `orchestrator/tests/smoke`: minimal host smoke checks.
- `tests/e2e`: cross-component integration tests.
