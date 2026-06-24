# Next Steps & Future Enhancements

The current implementation of CHAOS is a solid foundation for chaos engineering in containerized environments. The next steps involve:

## 1. Modern C++ Optimizations (C++20/23 Features)

Right now, concurrency and error handling are robust, but it can leverage modern C++ features to make the code faster, safer, and cleaner.

* **Migrate to std::jthread and std::stop_token:** ✅ **IMPLEMENTED**

    * The PerturbationEngine now uses `std::jthread` with `std::stop_token` for concurrent fault injection, with RAII join on destruction (no detached threads). The Web Server's run thread also uses `std::jthread` instead of `std::thread::detach()`, guaranteeing thread completion before `Server` destruction.

* **Adopt std::expected for Error Handling:** (Low priority — deferred indefinitely)

    * Current State: It is using try-catch blocks for control flow (e.g., catching ContainerEngineError).

    * The Improvement: C++23 introduced std::expected<T, E>. Exceptions should be for truly exceptional, unrecoverable crashes. For expected failures (like a network timeout to the Docker API or a missing container), returning std::expected<TargetState, ErrorCode> makes the control flow predictable, avoids the performance overhead of stack-unwinding during exceptions, and forces the caller to handle the error at compile time.

    * Status: Low-impact refactor on a working codebase with a single exception hierarchy. The ergonomic and compile-time correctness gains don't justify the churn. Revisit if the codebase grows multiple error origins or a second container backend.


## 2. Architectural Refinements

As system grows, preventing class bloat and tight coupling will be biggest challenge.

* **Interface Segregation Principle (ISP) for Docker:** (Low priority — deferred indefinitely)

    * Current State: IContainerEngine handles listing containers, killing them, stopping them, observing metrics, and updating resources (18 methods).

    * The Improvement: Break this massive interface down. Create IResourceLimiter (for CPU/RAM), ILifecycleManager (for Stop/Kill), and ITelemetryProvider (for Observation). This ensures that the ObservabilityEngine only has access to telemetry methods, preventing it from accidentally killing a container.

    * Status: Splitting adds indirection with marginal safety gain since there is only one implementation (DockerClient). The current interface is well-understood and tested. Revisit when a second backend (e.g., Kubernetes) is added and the consumer separation becomes meaningful.

* **Expanded Fault Library (Disk I/O Throttling):**

    * Extending the `PerturbationFactory` to include disk I/O disruptions, such as block I/O throttling (`BlkioWeight` via cgroups) and simulated disk failures (e.g., making `write()` return `ENOSPC` inside the container namespace). This will require implementing new classes that adhere to the `IPerturbation` interface and updating the manifest schema to support these new fault types.

## 3. Advanced SRE / Chaos Features

To compete with tools like Chaos Mesh or Gremlin, CHAOS needs to expand its blast radius and fault library.

* **Advanced Network Chaos (NetEm):** ✅ **IMPLEMENTED**

    * Three `tc netem`-based perturbations (`network_delay`, `traffic_corruption`) and one `iptables`-based perturbation (`network_cutoff`) inject latency, packet loss, corruption, duplication, and traffic blocking directly into the target container's network namespace via `nsenter`. A fifth perturbation (`packet_flood`) uses `AF_PACKET` raw sockets via `setns()` for packet flooding. All are instantiated through `PerturbationFactory`.

* **Blast Radius Controls & Dry-Run Mode:**
    * In production, chaos tools need safety nets. Implement a --dry-run flag that parses the manifest, verifies the target exists, calculates the required memory/CPU, but doesn't actually execute the fault. Add a "max targets" limit to prevent a regex from accidentally taking down 100 containers instead of 1.

* **CI/CD Reporting (JUnit XML / JSON):**
    * The Improvement: Add a ReportingEngine. When ValidationEngine finishes, dump the results into a standard JUnit XML file. This allows GitHub Actions, GitLab CI, or Jenkins to automatically read the test results and fail the deployment pipeline if the chaos expectations are not met.

## 4. Observability Enhancements

* **Live UI Integrations (Web):** ✅ **IMPLEMENTED**
    * The `StateBroadcaster` now streams real-time telemetry via SSE (Server-Sent Events) on `GET /events`. The Web UI has a Live Monitor panel that visualizes container stress (CPU, memory, status) in real time during a run.

* **Integration with OpenTelemetry:** ✅ **IMPLEMENTED**
    * A minimal `OtlpExporter` class builds OTLP Logs JSON payloads and exports them over HTTP to configurable endpoints. Configure via `CHAOS_OTLP_ENDPOINT` environment variable. Supports resource attributes, severity levels, and run_id tracking.

* **Advanced Metric Extraction (ObservationNeeds):** 
    * Moving beyond basic pass/fail validation. The goal is to capture granular metrics (e.g., capturing the exact CPU spike during the "Garbage Collection Death Spiral" right before an `OOMKilled` event occurs) to provide developers with deep diagnostic insights.

## 5. Visual Chaos Configuration (GUI Test Builder)

Relying on users to manually write a manifest.json is error-prone. To make CHAOS accessible to QA teams and developers, the Web Server should be upgraded from a simple monitoring dashboard into an interactive test builder.

* **The Improvement:** ✅ **IMPLEMENTED** — Built a dynamic Manifest Builder in the Web UI (form-based with type-aware parameter inputs) to configure, validate, and launch chaos experiments visually.

* **Live Target Discovery:** ✅ **IMPLEMENTED** — The UI fetches available targets via `GET /api/targets` from the backend's `IContainerEngine`. Users select a running container from a live dropdown.

* **Dynamic Schema Validation:** ✅ **IMPLEMENTED** — The UI queries the backend via `GET /api/limits` for the host's CPU cores and memory. Perturbation inputs are constrained to valid ranges.

* **One-Click Execution & Feedback Loop:** ✅ **IMPLEMENTED** — The UI sends the JSON payload to `POST /api/run` (async, returns 202). The Web UI immediately subscribes to `GET /events` (SSE), transitioning from "Test Builder" to "Live Monitor" mode with real-time state updates and validation results.

## 6. In-Container Fault Injection (The PID 1 Advanced Wrapper)

While the current architecture brilliantly uses the Docker API to apply constraints from the outside (infrastructure level), injecting a custom PID 1 wrapper into the container opens the door to application-level chaos.

* **The Improvement:** Evolve the initial wrapper concept into a lightweight, statically compiled agent (written in C++ or Rust) that is injected into the container at startup. The orchestrator communicates with this wrapper via a mounted Unix socket or gRPC.

* **Targeted Process Killing:** Instead of killing the entire container via Docker, the wrapper can target specific child processes (e.g., killing a background worker thread while leaving the main web server alive) to test partial degradation.

* **Application-Level Resource Exhaustion:** The wrapper can maliciously allocate heap memory from inside the container or burn CPU cycles internally. This triggers the application's actual Garbage Collector or Out-Of-Memory (OOM) handler more authentically than an external cgroup limit.

* **System Call Interception (Syscall Faults):** By using ptrace or eBPF within the wrapper, you can intercept the application's system calls. You can simulate a full disk by making the write() syscall return an ENOSPC error, or simulate file corruption by returning garbage data on read(), all without touching the host's actual hard drive.

## 7. Run History ✅ IMPLEMENTED

Run history persistence is implemented. Every completed or aborted run is saved to disk as a JSON file with full time-series data, manifest, and validation results. The Web UI has a History button for browsing and replaying past runs. The CLI has `chaos history` and `chaos history <id>` commands with `--json` output support.

## 8. Kubernetes Backend

The only `IContainerEngine` implementation today is `DockerClient`. To expand the blast radius to production Kubernetes clusters, a second engine backend is needed.

* **The Improvement:** Implement a `KubernetesEngine` (or `K8sEngine`) class that satisfies the `IContainerEngine` interface using the Kubernetes API. Target pods by label selector, inject faults into individual containers within a pod, and respect pod lifecycle (e.g., avoid killing the only replica of a Deployment without explicit opt-in).

* **Scope:** Start with a read-only `/api/targets` equivalent (list pods in a namespace matching a label selector), then add the perturbation primitives (kill pod, inject network chaos via ephemeral debug containers, resource limits via `kubectl set resources`). The existing manifest schema and `PerturbationFactory` require no changes — only the transport layer needs a new backend.

## 9. Web UI Authentication & API Key

The Web UI currently has no authentication mechanism, making it unsuitable for multi-user or production deployments.

* **The Improvement:** Add an optional API key (`CHAOS_API_KEY` env var or `--api-key` flag) that gatekeeps all `/api/*` endpoints via middleware. Unauthenticated requests receive 401. The frontend prompts for the key on first connection and stores it in session storage.
