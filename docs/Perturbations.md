# CHAOS Perturbations & Expectations Reference

This document serves as the user manual for constructing JSON manifests used by the CHAOS orchestrator. It outlines what each perturbation does, the required parameters, and how to assert system expectations.

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
  - `quota` (integer): The amount of CPU time (in microseconds) the container can use per period.
  - `period` (integer): The length of the CPU accounting period (in microseconds).
- **Under the hood**: Uses the Docker Update API to natively restrict the CPU scheduling ratio.
- **Effect**: Execution of the container is artificially paused or slowed down to strictly enforce the CPU ratio (quota/period). 

### 4. `network_delay`
- **Description**: Introduces artificial latency to all outgoing network traffic.
- **Parameters**:
  - `delay_ms` (integer): Latency to add to each packet in milliseconds.
- **Under the hood**: Executes `tc qdisc add dev eth0 root netem delay {delay_ms}ms` inside the target's network namespace.
- **Effect**: Any network-bound operations performed by the container will experience delays, simulating a slow or congested network connection.

### 5. `network_cutoff`
- **Description**: Severs the container from the network.
- **Parameters**: None.
- **Under the hood**: Applies `iptables -A INPUT -j DROP` (or similar interface lockdown) inside the container namespace.
- **Effect**: The container becomes completely unreachable from the outside and cannot receive new connections, though it may still appear as running.

### 6. `garbage_packet`
- **Description**: Floods the container with unexpected, malformed data.
- **Parameters**: None.
- **Under the hood**: Injects randomized, noisy UDP payload packets directly to listening ports inside the target container.
- **Effect**: Protocol parsers and servers within the container are stressed and must gracefully reject invalid data without crashing.

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
- **Description**: Scans the container's standard output and standard error for an exact text match.
- **Parameters**:
  - `substring` (string): The exact string of text that *must* appear in the logs.

### 4. `log_not_contains`
- **Description**: Scans the container's output to ensure a specific text was *never* logged.
- **Parameters**:
  - `substring` (string): The exact string of text that *must not* appear in the logs (useful for verifying error messages were not printed).
