# Examples

This directory contains runnable examples for the CHAOS orchestrator and the
demo applications used by them.

## Requirements

- Docker, with permission to create and remove containers and networks.
- `curl` for the API helper scripts.
- A built CHAOS binary for running manifests. The default path used by
  `run-defense-demo.sh` is `../build/dev-linux-clang/orchestrator/chaos`.
- `jq` is optional; `api-showcase.sh` uses it to pretty-print JSON responses.

The perturbations can change container limits and host networking. Run them
only in an environment where those privileged operations are expected.

## Standard demo

Run `./run-demo.sh` from this directory to build the `chaos-demo-target` image
and start `chaos-demo-1`. The target publishes its HTTP server on
`http://127.0.0.1:8000`.

Paste any manifest from `demo/` into the web UI, or run it with the CLI:

```bash
chaos run examples/demo/01-resource-exhaustion.json
```

The dashboard uses port `8080`; the target application uses port `8000`.
Manifest HTTP expectations must use the target's port, `8000`.

### Standard demo manifests

| Manifest | Scenario |
| --- | --- |
| `demo/01-resource-exhaustion.json` | Applies a CPU cap and a 256 MiB memory cap while checking that the container remains running and responds to `/ping`. |
| `demo/02-network-cutoff.json` | Cuts the target's network. The container should remain running, but the HTTP check is intentionally expected to fail while the cutoff is active. |
| `demo/03-garbage-flood.json` | Sends a packet flood while checking the target's liveness and `/ping` endpoint. |
| `demo/04-kill.json` | Kills the target and expects it not to be running. Run this last or restart the target afterwards. |
| `demo/05-traffic-corruption.json` | Applies packet corruption, loss and duplication while checking `/ping`. |

### API helper scripts

These scripts call the standard target directly. Set `BASE_URL` when the
target is not available at `http://127.0.0.1:8000`.

| Script | Usage | Purpose |
| --- | --- | --- |
| `api-network-delay.sh` | `./api-network-delay.sh` | Calls `/ping` once. |
| `api-cpu-cap.sh` | `./api-cpu-cap.sh [run\|start\|stop] [count]` | Runs timed CPU work, starts continuous CPU workers, or stops them. `SECONDS` and `WORKERS` customize the workload. |
| `api-memory-cap.sh` | `./api-memory-cap.sh [count]` | Calls `/allocate` repeatedly to allocate memory. |
| `api-showcase.sh` | `./api-showcase.sh` | Runs a live sequence of API calls, prints responses and latency statistics, and shows container logs when available. `PORT`, `BASE_URL`, `CONTAINER_NAME`, `LOG_LINES`, `CPU_COUNT`, `ALLOC_COUNT`, `TIMEOUT` and `SLEEP_BETWEEN` customize it. |

## Defense demo

Run `./run-defense-demo.sh` to build the demo image, create the
`chaos-defense-net` Docker network, and start two containers:

- `chaos-demo-api` on host port `8000`
- `chaos-demo-downstream` on host port `8001`

The API calls the downstream service through the Docker network. The setup
also enables the networking configuration required by `packet_flood` and
pre-allocates memory in the API container.

The manifests in `defense-demo/` are designed for this setup:

| Manifest | Scenario |
| --- | --- |
| `defense-demo/01-gamegrid-flood.json` | Combines packet flooding and traffic corruption, then checks liveness, HTTP status and latency. |
| `defense-demo/02-mediwatch-memory-cap.json` | Applies a 32 MiB memory cap to the API. This can trigger an OOM kill; recreate the containers before running another case. |
| `defense-demo/03-payflow-cascade.json` | Combines network delay and CPU capping, then checks the downstream call latency. |

Run a defense manifest with:

```bash
sudo ../build/dev-linux-clang/orchestrator/chaos run \
  examples/defense-demo/01-gamegrid-flood.json
```

The defense setup uses privileged Docker and host networking operations. The
script removes the demo containers and network automatically if setup fails.
For manual cleanup:

```bash
docker rm -f chaos-demo-api chaos-demo-downstream
docker network rm chaos-defense-net
```

## Demo target

`demo-target/Dockerfile` builds the Python target image used by both setup
scripts. `server.py` serves the standard API on port `8000`, including
`/ping`, `/allocate`, and CPU workload endpoints. `server_downstream.py`
serves the downstream service on port `8001` for the defense demo.

## Reverting a memory cap

`revert-memory-cap.sh [container-name]` removes the effective cgroup v2 memory
and swap limits from a running container. It defaults to `chaos-demo-1` and
may require `sudo` when the cgroup files are not writable.
