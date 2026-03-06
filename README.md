# CHAOS: Resilience Validation Platform

CHAOS is a software engineering tool developed in C++23, designed for deterministic robustness verification in containerized applications.

Unlike distributed chaos orchestrators, CHAOS specializes in unit validation of components. The system interacts directly with the Linux kernel (using Cgroups v2 and Network Namespaces) to subject a specific container to degraded execution conditions.


## Technical Objectives

- Fault Isolation: Identify how a single process handles critical resource scarcity (CPU/RAM).
- Recovery Validation: Empirically verify reconnection and failover mechanisms in the face of induced network latencies or outages.
- Stress Monitoring: Provide real-time metrics on the behavior of the “victim” under controlled stress scenarios.

## Architecture Overview
![Diagram of the system architecture](docs/images/architectural-design.png)

## Local build (portable)

To avoid exporting `VCPKG_ROOT` manually, use:

`bash scripts/cmake-local.sh all`

The script auto-detects `vcpkg` in this order:

1. `VCPKG_ROOT` (if already set and valid)
2. `./vcpkg`
3. `/vcpkg`

You can run a single stage too:

- `bash scripts/cmake-local.sh configure`
- `bash scripts/cmake-local.sh build`
- `bash scripts/cmake-local.sh test`

## Run the CLI

After building, run:

- `./build/debug-clang/chaos help`
- `./build/debug-clang/chaos list`
- `./build/debug-clang/chaos stop <container_id>`
- `./build/debug-clang/chaos kill <container_id>`

## Run the Web UI

Start the server:

- `./build/debug-clang/chaos serve --port 8080`

Then open:

- `http://127.0.0.1:8080`
