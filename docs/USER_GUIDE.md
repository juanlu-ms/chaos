# CHAOS User Guide

## Installation

### Supported Platforms

Releases ship a **dynamically linked x86_64 Linux binary**, built on Ubuntu 24.04 with Clang 18. It requires **glibc 2.38 or newer** and a **libstdc++ from GCC 13.2 or newer**. Check your system in one line:

```bash
ldd --version | head -1     # needs 2.38 or newer
```

**Runs on:**

- Ubuntu 24.04 LTS and newer — and derivatives such as Linux Mint 22+ and Pop!_OS 24.04+
- Debian 13 "trixie" and newer
- Fedora 39 and newer
- RHEL / Rocky / AlmaLinux 10
- Arch, Manjaro, openSUSE Tumbleweed and other rolling distributions

**Does not run on** — build from source (Option B) instead, there is no compatibility shim:

- Ubuntu 22.04 LTS and older (glibc 2.35)
- Debian 12 "bookworm" (glibc 2.36)
- RHEL / Rocky / AlmaLinux 9 and Amazon Linux 2023 (glibc 2.34)
- Alpine and other musl-based distributions (no glibc at all)
- Any non-x86_64 machine — ARM (AWS Graviton, Ampere, Raspberry Pi, Docker Desktop on Apple Silicon) fails with `Exec format error`

### Option A: Download Pre-built Binary (Recommended)

Grab the latest binary from [GitHub Releases](https://github.com/juanlu-ms/chaos/releases). Each release tagged `v*` publishes `chaos-<version>-linux-x86_64` alongside a `.sha256` checksum.

```bash
sha256sum -c chaos-0.4.3-linux-x86_64.sha256   # verify the download
chmod +x chaos-0.4.3-linux-x86_64
sudo mv chaos-0.4.3-linux-x86_64 /usr/local/bin/chaos   # put it on your PATH
sudo chaos help
```

### Option B: Build from Source

See the [Development Guide](DEVELOPMENT.md#building-from-source) for full build instructions.

```bash
cmake --preset dev-linux-clang && cmake --build --preset debug -- -j$(nproc)
sudo ./build/dev-linux-clang/orchestrator/chaos help
```

### Runtime Prerequisites

However you install it, `chaos` drives the host's own networking and container tooling rather than bundling its own. The following must be present:

| Requirement | Package | Used for |
|-------------|---------|----------|
| Docker daemon on **cgroup v2** | `docker` | Container control via `/var/run/docker.sock`; CPU and memory metrics read from `/sys/fs/cgroup` |
| `nsenter` | `util-linux` | Entering the target container's network namespace |
| `tc` | `iproute2` | `network_delay`, `traffic_corruption` |
| `iptables` | `iptables` | `network_cutoff` |
| `ping` | `iputils` | Reachability probing during observation |

On Debian/Ubuntu: `sudo apt-get install util-linux iproute2 iptables iputils-ping`.

### Privilege Requirements

Network perturbations (`network_delay`, `network_cutoff`, `packet_flood`, `traffic_corruption`) enter the target container's network namespace (via `nsenter` or `setns()`) to inject faults — `tc`/`iptables` rules for most, raw `AF_PACKET` sockets for `packet_flood`. This requires **root privileges** — run the orchestrator with `sudo` or as root. CPU and memory perturbations work without root.

---

## CLI Reference

After building or downloading:

```bash
sudo chaos help
sudo chaos list
sudo chaos stop <container_id>
sudo chaos kill <container_id>
sudo chaos run <manifest.json>
sudo chaos history [--json]
sudo chaos history <run-id> [--json]
sudo chaos history clear
```

### Log Level Flags

Apply to all commands:

| Flag | Effect |
|------|--------|
| `-v`, `--verbose` | Enable debug logging |
| `-q`, `--quiet` | Suppress non-warning logs |
| `--log-level LEVEL` | Set log level (trace\|debug\|info\|warn\|error\|critical\|off) |
| `--no-color` | Disable colored output |

### Run-specific Options

| Flag | Effect |
|------|--------|
| `--json` | Emit machine-readable JSON to stdout instead of the human report |
| `--output PATH` | Write the JSON report to PATH (use `-` for stdout) |

### Run History Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CHAOS_HISTORY_DIR` | `~/.chaos/history/` | Directory for run history JSON files |
| `CHAOS_HISTORY_MAX` | `50` | Maximum number of runs to retain |

### CI & JSON Output

The `--json` flag produces a single JSON object containing the full run result:

```json
{
  "passed": false,
  "manifest_name": "MediWatch — Memory Cap",
  "target_id": "chaos-demo-api",
  "duration_s": 16.1,
  "started_at": "2026-06-20T22:35:19Z",
  "results": [
    {"type": "container_not_running", "passed": true, "message": "Container is not running"},
    {"type": "http_status", "passed": false, "message": "HTTP GET ... failed: Connection timed out"}
  ]
}
```

**Exit codes:** `0` = all expectations passed; `1` = any expectation failed or run error.

```bash
sudo chaos run --json manifest.json | jq .passed   # CI integration
```

### OpenTelemetry Export

Set `CHAOS_OTLP_ENDPOINT` to export run events to an OTLP-compatible backend (Grafana, Datadog, etc.). The exporter sends JSON-encoded OTLP Logs over HTTP.

---

## Web UI

Start the server:

```bash
sudo chaos serve --port 8080
```

Then open `http://127.0.0.1:8080`.

The Web UI has a 3-step flow:

1. **Configure** — select target, set duration, add perturbations with parameters, configure expectations, click "Run Chaos Test"
2. **Monitor** — watch live line charts (CPU, Memory, Network I/O), container info, and logs update in real time via SSE. Click **Abort** to stop
3. **Results** — summary stats, timeline charts with Normal/Chaos/Recovery color zones, expectation validation results, run logs. Click **Run Again** or **Modify Manifest**

Switch between 11 built-in themes (Amber — the default — Dark, Cyber, Light, Matrix, Synthwave, Solarized, One Dark, Tokyo Night, Nord, Catppuccin) from the topbar dropdown. Your preference is saved to localStorage.

### API Endpoints

| Method | Route | Description |
|--------|-------|-------------|
| GET | `/api/status` | Service status `{project, status}` |
| GET | `/api/containers` | List available containers `[{id, name, state}]` |
| GET | `/api/limits` | System limits `{cpu_cores, memory_total_mb, perturbation_limits}` |
| POST | `/api/run` | Async run — returns 202, streams state via SSE |
| POST | `/api/run/abort` | Abort the currently running test |
| GET | `/api/history` | List completed runs `[{id, started_at_unix, status, ...}]` |
| GET | `/api/history/{id}` | Full run record (manifest, samples, logs, results) |
| DELETE | `/api/history/{id}` | Delete a single run |
| DELETE | `/api/history` | Clear all run history |
| GET | `/api/events` | SSE stream (`event: state` / `event: complete` / `event: error`) |
| POST | `/api/containers/{id}/stop` | Stop a container |
| POST | `/api/containers/{id}/kill` | Kill a container |
| GET | `/api/containers/{id}/logs` | Fetch container logs |

---

## Manifest Reference

For the complete perturbation and expectation schema, see [Perturbations & Expectations Reference](Perturbations.md).

---

## Run History

Each completed chaos run is automatically persisted to `~/.chaos/history/` (override: `CHAOS_HISTORY_DIR`). Runs are capped at the 50 most-recent (override: `CHAOS_HISTORY_MAX`). Each record includes the full manifest, time-series samples, log tail, and validation results.

Browse past runs via:

- **Web UI:** History button in the topbar → select a run to replay the full results view
- **CLI:** `chaos history` (list), `chaos history <id>` (detail), `chaos history clear` (purge)

Run history works for both Web and CLI runs.
