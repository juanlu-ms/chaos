# Case Studies — TFG Defense Demo

## Demo Architecture

The demo uses two Docker containers communicating over a custom network (`chaos-defense-net`):

| Container | Role | Port | Endpoints |
|-----------|-----|--------|-----------|
| `chaos-demo-api` | Main service | 8000 | `/ping`, `/allocate`, `/cpu`, `/call-downstream` |
| `chaos-demo-downstream` | Downstream dependency | 8001 | `/ping`, `/data` |

The API server exposes `/call-downstream`, which internally makes an outbound HTTP request to `chaos-demo-downstream:8001/data`. Both containers receive `--cap-add=NET_ADMIN` to allow network fault injection.

---

## Case 1: MediWatch — Memory Leak

**Narrative:** A telemedicine company deploys patient monitors on edge devices (Raspberry Pi) in an ICU. In development, all tests pass with 32GB RAM. In production, the devices have 512MB and the system randomly reboots after 8 hours of operation.

**Manifest:** `examples/defense-demo/01-mediwatch-memory-cap.json`

```json
{
  "test_name": "MediWatch — Memory Cap (Fuga de Memoria)",
  "target": { "id": "chaos-demo-api" },
  "duration_s": 12,
  "perturbations": [
    {
      "type": "memory_cap",
      "parameters": { "limit_bytes": "33554432" }
    }
  ],
  "expectations": [
    { "type": "container_not_running" },
    {
      "type": "http_status",
      "parameters": { "port": "8000", "path": "/ping", "expected_status": "200" }
    }
  ]
}
```

### What CHAOS Does

1. The `run-defense-demo.sh` script pre-leaks 30MB in the API by calling `/allocate` 3 times (10MB each)
2. CHAOS applies the 32MB limit via the Docker API — sets `memory.limit_in_bytes` in the container's cgroups
3. The container already consumes ~18MB baseline + 30MB leaked = 48MB > 32MB limit
4. The Linux kernel's OOM killer terminates the process immediately when the limit is applied

### Expected Results

| Expectation | Result | Reason |
|-------------|--------|--------|
| `container_not_running` | PASS | CHAOS detects the container transitioned to `exited` state |
| `http_status 200` on `/ping` | FAIL | The container is dead, cannot respond |

### Key Concept

**Linux cgroups and the OOM killer.** When a process exceeds `memory.limit_in_bytes`, the kernel invokes the OOM killer, which sends `SIGKILL` to the process. CHAOS compresses 8 hours of production degradation into 10 seconds of local validation.

### What the Developer Learns

Their application has a memory leak invisible in development environments with abundant RAM. They must change their memory allocation strategy — use streaming or chunked processing instead of loading entire datasets into RAM.

---

## Case 2: PayFlow — Cascade Failure

**Narrative:** A payment gateway (PayFlow) calls an external fraud-detection service via HTTP. In staging, it shows 99.9% availability. During Black Friday, transactions start timing out without any service appearing as "down". The container is `running` but the system doesn't work.

**Manifest:** `examples/defense-demo/02-payflow-cascade.json`

```json
{
  "test_name": "PayFlow — Latencia en Cascada (network_delay + cpu_cap)",
  "target": { "id": "chaos-demo-api" },
  "duration_s": 15,
  "perturbations": [
    {
      "type": "network_delay",
      "parameters": { "delay_ms": "1200" }
    },
    {
      "type": "cpu_cap",
      "parameters": { "cpu_cores": "0.5" }
    }
  ],
  "expectations": [
    { "type": "container_running" },
    {
      "type": "http_latency",
      "parameters": { "port": "8000", "path": "/call-downstream", "max_latency_ms": "500" }
    }
  ]
}
```

### What CHAOS Does

1. **`network_delay`**: runs `nsenter -t <pid> -n tc qdisc replace dev eth0 root netem delay 1200ms` inside the API container's network namespace. This adds 1200ms of latency to every TCP packet leaving `eth0`.

2. **`cpu_cap`**: sets `CpuQuota=50000` / `CpuPeriod=100000` via the Docker API, limiting the container to 50% of one CPU core.

3. **`http_latency` with continuous validation**: this expectation is automatically evaluated every 500ms during the chaos phase (marked as `continuous: true` by default). The `ObservationLoop` makes real HTTP requests to the container and measures latency.

### Why Latency Fails

The path of a `GET /call-downstream` request during chaos:

```
CHAOS ──[ingress, 0ms]──> API ──[egress, +1200ms]──> Downstream ──[+50ms /data]──> API ──[egress, +1200ms]──> CHAOS
                                                                           (ingress, 0ms)
```

- **Ingress** (entering the container): no delay — `tc netem` on the root qdisc only affects outbound traffic
- **Egress** (leaving the container): +1200ms each time
- The downstream call (`/data`) takes ~50ms
- Total latency: 0 + 1200 + 50 + 1200 ≈ **2450ms**
- With CPU at 50%, processing overhead adds more latency
- `max_latency_ms: 500` → 2450ms >> 500ms → **FAIL**

### Continuous Validation Mechanism

1. `ObservationLoop::metricsThreadFn()` calls `validation::validate()` every 500ms during the `chaos` phase
2. `TargetState` includes `container_ip` (172.18.0.3) — the `httplib` HTTP client connects directly to the container
3. The failure is recorded via `SharedState::addContinuousFailure("http_latency")`
4. In `ChaosRunner::finalize()`, even if the final check passes (because the delay was already reverted in the recovery phase), the result is overridden: *"Passed final validation but failed mid-run continuous check"*

### Expected Results

| Expectation | Result | Reason |
|-------------|--------|--------|
| `container_running` | PASS | The container survives — the service doesn't crash, it just degrades |
| `http_latency < 500ms` | FAIL | Real latency ~2450ms, far above the 500ms limit |

### Key Concept

**Linux network namespaces** (`nsenter` + `tc netem`), **CPU cgroups**, and **cascading failures between microservices**. A service doesn't need to crash to be broken — being slow and causing timeouts in its dependents is enough.

### What the Developer Learns

Their API has no timeout configured on the outbound HTTP call to the fraud service. Without a timeout, the main thread blocks waiting for a response that never arrives, accumulating requests and exhausting the thread pool. They must implement timeouts, circuit breakers, and backpressure.

---

## Case 3: GameGrid — DDoS Attack

**Narrative:** A game company (GameGrid) operates multiplayer servers with a constant update loop (ticks), sensitive to CPU performance. Every launch weekend, their servers collapse under player load. Yet their load tests show they can handle 10k concurrent connections. What changes in production?

**Manifest:** `examples/defense-demo/03-gamegrid-flood.json`

```json
{
  "test_name": "GameGrid — Ataque DDoS (packet_flood + traffic_corruption)",
  "target": { "id": "chaos-demo-api" },
  "duration_s": 12,
  "perturbations": [
    {
      "type": "packet_flood",
      "parameters": { "rate": "5000", "packet_size": "512" }
    },
    {
      "type": "traffic_corruption",
      "parameters": { "corrupt_pct": "10%", "loss_pct": "10%", "duplicate_pct": "5%" }
    }
  ],
  "expectations": [
    { "type": "container_running" },
    {
      "type": "http_status",
      "parameters": { "port": "8000", "path": "/ping", "expected_status": "200" }
    },
    {
      "type": "http_latency",
      "parameters": { "port": "8000", "path": "/ping", "max_latency_ms": "100" }
    }
  ]
}
```

### What CHAOS Does

1. **`packet_flood`**: opens a raw socket (`AF_PACKET`) inside the container's network namespace via `setns()`. Builds valid Ethernet+IP+TCP frames targeting the container's own IP with:
   - Random source MAC addresses
   - Spoofed source IPs
   - Random source TCP ports
   - Destination port 8000
   - Random garbage payload

   Sends 5000 packets per second with a frame size of 512 bytes.

2. **`traffic_corruption`**: applies `tc qdisc add dev eth0 root netem corrupt 10% loss 10% duplicate 5%` inside the network namespace, corrupting/dropping/duplicating legitimate packets.

### Combined Effect

- The flood of 5000 pps with 512-byte frames (~20 Mbps) saturates the kernel's SYN queue and consumes CPU through interrupts
- CPU spikes from network interrupt handling and TCP response construction
- 10% packet loss + 10% corruption cause legitimate TCP connections to fail or require retransmissions
- 5% duplication generates additional traffic and confuses the application layer
- `http_latency` is validated continuously (every 500ms) during the chaos phase — detects degradation by measuring real response time
- The container stays `running` but the HTTP service becomes **degraded**: latency far above 100ms, even if the status code may occasionally be 200

### Expected Results

| Expectation | Result | Reason |
|-------------|--------|--------|
| `container_running` | PASS | The container survives the volumetric attack |
| `http_status 200` on `/ping` | FAIL (intermittent) or PASS | The service responds erratically; may return 200 occasionally |
| `http_latency < 100ms` on `/ping` | FAIL | Even if `http_status` returns 200, real latency is far above 100ms under attack |

### Recovery Phase

After 12 seconds of chaos:
1. The flood thread stops (`stop_token` requested)
2. The raw socket is closed
3. The `tc netem` rules are removed with `tc qdisc del dev eth0 root netem`
4. The `PerturbationEngine` guarantees reversion via RAII (`waitForTeardown()`)
5. The container returns to normal operation

In the Web UI, the timeline charts show three colored zones:
- **Green** (Normal): normal traffic and latency
- **Red** (Chaos): network traffic spike, CPU at 100%, latency skyrocketing
- **Blue** (Recovery): metrics returning to baseline

### Key Concept

**Raw sockets** (`AF_PACKET` + `setns()`), **address spoofing**, and **kernel-level network congestion**. Continuous validation of `http_latency` (every 500ms during chaos) proves that a service can be *running* but *degraded* — CHAOS exposes this difference by measuring real latency under attack.

### What the Developer Learns

Their game server has no protection against malicious traffic at the network level. They must implement rate limiting, SYN cookies, and possibly a reverse proxy with filtering capabilities. Automatic recovery works — the server returns to normal after the attack — but players experienced lag, disconnections, and rubber-banding during the incident.

---

## Summary

| Case | Perturbations | Expectations | Linux Concept |
|------|--------------|-------------|----------------|
| MediWatch | `memory_cap` (32MB) | `container_not_running` PASS, `http_status` FAIL | Cgroups v1/v2, OOM killer |
| PayFlow | `network_delay` (1200ms) + `cpu_cap` (0.5 cores) | `container_running` PASS, `http_latency` FAIL | Network namespaces, tc netem, CPU cgroups |
| GameGrid | `packet_flood` (5000 pps, 512B) + `traffic_corruption` (10% / 10% / 5%) | `container_running` PASS, `http_status` FAIL, `http_latency` FAIL | Raw sockets (AF_PACKET), setns(), tc netem |

**Unifying principle:** Resilience is not a feature — it's a property you validate. CHAOS lets you validate it at development time, before failures reach production.
