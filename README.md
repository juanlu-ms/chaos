# CHAOS: Chaos Handling And Observability System

CHAOS is a C++23 resilience validation tool for containerized systems. It injects controlled faults (CPU caps, memory limits, network degradation, process kills) into Docker containers and validates recovery against declared expectations. Designed for deterministic local runs and CI integration.

## Quick Start

**Download a pre-built binary** from [GitHub Releases](https://github.com/juanlu-ms/chaos/releases), or build from source:

```bash
# Build (see docs/DEVELOPMENT.md for prerequisites)
git submodule update --init --recursive
cmake --preset dev-linux-clang && cmake --build --preset debug -- -j$(nproc)
```

## Usage

```bash
sudo chaos serve --port 8080          # Launch the Web UI
sudo chaos run manifest.json          # Run a chaos test from a manifest
sudo chaos history                    # Browse past runs
```

Network perturbations require root. See the [User Guide](docs/USER_GUIDE.md) for full CLI reference, logging flags, API endpoints, and CI/JSON output.

## Key Features

- **6 perturbation types** — `cpu_cap`, `memory_cap`, `kill`, `network_delay`, `network_cutoff`, `packet_flood`, `traffic_corruption`
- **6 expectation types** — container state, log contents, HTTP status, and latency assertions with continuous validation
- **Concurrent execution** — perturbations run on `std::jthread` with `std::stop_token` for thread-safe cancellation and RAII join
- **Web UI** — dark-themed 3-step SPA (Configure → Monitor → Results) with live Recharts charts via SSE
- **Run history** — automatic persistence of runs with time-series data, browseable and replayable via Web UI or CLI
- **OpenTelemetry export** — optional OTLP Log export to Grafana, Datadog, etc.
- **Graceful interruption** — SIGINT reverts all applied faults before exit

## Documentation

| Document | Description |
|----------|-------------|
| [User Guide](docs/USER_GUIDE.md) | Installation, CLI usage, Web UI, API endpoints, CI output |
| [Perturbations & Expectations](docs/Perturbations.md) | JSON manifest schema reference |
| [Development Guide](docs/DEVELOPMENT.md) | Build commands, test suites, repo layout, formatting |
| [Architecture](docs/ARCHITECTURE.md) | Design decisions, concurrency models, patterns |
| [Roadmap](docs/ROADMAP.md) | Planned features and future enhancements |
