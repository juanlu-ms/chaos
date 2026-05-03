# CHAOS Perturbations & Expectations Reference

This document serves as the user manual for constructing JSON manifests used by the CHAOS orchestrator. It outlines what each perturbation does, the required parameters, and how to assert system expectations.

## Concurrent Architecture

The perturbation runtime executes actions concurrently and coordinates them through a central scheduler.

- The PerturbationEngine acts as the thread scheduler for all perturbations.
- The perturbation factory is decoupled from the engine so creation and execution are separate concerns.
- Individual perturbations run concurrently via `std::async`.
- The engine uses active polling to track progress and publishes updates through the StateBroadcaster.
- Cancellation and interrupts use `condition_variable::wait_for` to allow quick exits, and a SIGINT triggers an early revert to unwind running perturbations.

## 💥 Perturbations

Perturbations are the fault injection mechanisms applied to a target container. They are defined in the `perturbations` array of the JSON manifest.

### 1. `kill`
- **Description**: Instantly terminates the container.
- **Parameters**: None.
- **Under the hood**: Calls the container engine's native `kill` or `stop` mechanism.
- **Effect**: The container process is sent a fatal signal (like `SIGKILL`) and transitions to an exited state immediately.

### 2. `memory_cap`
- **Description**: Enforces a strict physical memory limit.
- **Parameters**:
  - `limit_bytes` (integer): The maximum amount of RAM the container is allowed to use.
- **Under the hood**: Uses the Docker Update API to natively limit container memory allocation.
- **Effect**: If the container attempts to allocate memory beyond this limit, the kernel's OOM (Out-Of-Memory) killer will terminate the offending process.

### 3. `cpu_cap`
- **Description**: Throttles the CPU scheduling time available to the container.
- **Parameters**:
  - `cpu_cores` (integer): A raw quota value representing the CPU cap. Must be an integer (e.g., `50000`, `200000`).
- **Under the hood**: Uses the Docker Update API to enforce a CPU limit using a fixed period of `100000` and the configured `cpu_cores` as the quota.
- **Effect**: Execution of the container is artificially paused or slowed down to enforce the configured CPU quota limit.

### 4. `network_delay`
- **Description**: Introduces artificial latency to all outgoing network traffic.
- **Parameters**:
  - `delay_ms` (integer): Latency to add to each packet in milliseconds.
- **Under the hood**: Executes `tc qdisc add dev eth0 root netem delay {delay_ms}ms` inside the target's network namespace.
- **Effect**: Any network-bound operations performed by the container will experience delays, simulating a slow or congested network connection.

### 5. `network_cutoff`
- **Description**: Drops network traffic using iptables.
- **Parameters** (optional filters):
  - `dst_ip` (string): The destination IP address to drop.
  - `dst_port` (string): The destination port to drop.
  - `src_port` (string): The source port to drop.
  If no parameters are provided, it drops all traffic on `eth0`.
- **Under the hood**: Applies `iptables -A INPUT` and `OUTPUT` rules with `-j DROP` inside the container namespace, using the provided filters if any.
- **Effect**: The specified network traffic (or all traffic if unfiltered) is dropped, severing communication for those endpoints.

### 6. `garbage_packet`
- **Description**: Corrupts, drops, and duplicates network packets on the interface.
- **Parameters**: None.
- **Under the hood**: Executes `tc qdisc add dev eth0 root netem` with corruption, loss, and duplication rules (e.g., `corrupt 5% loss 5% duplicate 1%`).
- **Effect**: Network traffic experiences data corruption and packet loss, forcing protocol parsers and network stacks to handle malformed or dropped packets.

---

## ✅ Expectations

Expectations act as assertions after the perturbations run. They are defined in the `expectations` array of the JSON manifest. If any expectation fails, the orchestrator returns a non-zero exit code or an HTTP 422 error.

### 1. `container_running`
- **Description**: Asserts that the target container is still alive and running.
- **Parameters**: None.

### 2. `container_not_running`
- **Description**: Asserts that the target container has died, crashed, or exited.
- **Parameters**: None.

### 3. `log_contains`
- **Description**: Scans the container's standard output and standard error to check if a specific substring is present.
- **Parameters**:
  - `substring` (string): The substring of text that *must* appear in the logs.

### 4. `log_not_contains`
- **Description**: Scans the container's output to ensure a specific substring was *never* logged.
- **Parameters**:
  - `substring` (string): The substring of text that *must not* appear in the logs (useful for verifying error messages were not printed).

### 5. `http_status`
- **Description**: Makes an HTTP GET request to the target and verifies the response status code.
- **Parameters**:
  - `port` (string): The port to connect to.
  - `status` (string): The expected HTTP status code (e.g., `200`).
  - `path` (string, optional): The path to request (defaults to `/`).

### 6. `http_latency`
- **Description**: Makes an HTTP GET request to the target and verifies the response time is within bounds.
- **Parameters**:
  - `port` (string): The port to connect to.
  - `latency_ms` (string): The maximum acceptable latency in milliseconds.
  - `path` (string, optional): The path to request (defaults to `/`).
