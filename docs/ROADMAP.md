# Next Steps & Future Enhancements

The current implementation of CHAOS is a solid foundation for chaos engineering in containerized environments. The next steps involve:

## 1. Modern C++ Optimizations (C++20/23 Features)

Right now, concurrency and error handling are robust, but it can leverage modern C++ features to make the code faster, safer, and cleaner.

* **Migrate to std::jthread and std::stop_token:**

    * Current State: It is using std::async, a std::mutex, a std::condition_variable, and a boolean flag to handle task cancellation.

    * The Improvement: C++20 introduced std::jthread (Joining Thread), which has built-in cancellation support via std::stop_token. You can pass a std::stop_token directly into perturbation lambdas. When you call request_stop() on the parent thread, the condition variables inside the child threads wake up automatically. This eliminates the need for custom cancel_mutex_ and manual state flags, drastically reducing boilerplate and the risk of deadlocks.

* **Adopt std::expected for Error Handling:**

    * Current State: It is using try-catch blocks for control flow (e.g., catching ContainerEngineError).

    * The Improvement: C++23 introduced std::expected<T, E>. Exceptions should be for truly exceptional, unrecoverable crashes. For expected failures (like a network timeout to the Docker API or a missing container), returning std::expected<TargetState, ErrorCode> makes the control flow predictable, avoids the performance overhead of stack-unwinding during exceptions, and forces the caller to handle the error at compile time.


## 2. Architectural Refinements

As system grows, preventing class bloat and tight coupling will be biggest challenge.

* **Interface Segregation Principle (ISP) for Docker:**
    * Current State: IContainerEngine handles listing containers, killing them, stopping them, observing metrics, and updating resources.
    * The Improvement: Break this massive interface down. Create IResourceLimiter (for CPU/RAM), ILifecycleManager (for Stop/Kill), and ITelemetryProvider (for Observation). This ensures that the ObservabilityEngine only has access to telemetry methods, preventing it from accidentally killing a container.

* **State Machine for Container Lifecycle:**
    * Current State: Container states ("running", "exited") are likely handled as strings.
    * The Improvement: Use C++ std::variant and std::visit to build a strict finite state machine (FSM). This guarantees at compile-time that you cannot apply a MemoryCapPerturbation to a container that is currently in an Exited state.

* **Expanded Fault Library:** 
    * Extending the `PerturbationFactory` to include more complex, infrastructure-level disruptions, such as Disk I/O throttling (`BlkioWeight`), packet corruption, and simulated network partitions. This will require implementing new classes that adhere to the `IPerturbation` interface and updating the manifest schema to support these new fault types.

## 3. Advanced SRE / Chaos Features

To compete with tools like Chaos Mesh or Gremlin, CHAOS needs to expand its blast radius and fault library.

* **Advanced Network Chaos (NetEm):**
    * The Docker API can limit CPU and RAM, but it cannot inject network latency, packet corruption, or simulate dropped packets (at least not tested with real-world scenarios).
    * The Improvement: Implement a mechanism to inject tc (Traffic Control) and netem rules directly into the target container's network namespace (nsenter). This is the holy grail of chaos engineering and will allow you to simulate degraded networks, not just dead ones.

* **Blast Radius Controls & Dry-Run Mode:**
    * In production, chaos tools need safety nets. Implement a --dry-run flag that parses the manifest, verifies the target exists, calculates the required memory/CPU, but doesn't actually execute the fault. Add a "max targets" limit to prevent a regex from accidentally taking down 100 containers instead of 1.

* **CI/CD Reporting (JUnit XML / JSON):**
    * The Improvement: Add a ReportingEngine. When ValidationEngine finishes, dump the results into a standard JUnit XML file. This allows GitHub Actions, GitLab CI, or Jenkins to automatically read the test results and fail the deployment pipeline if the chaos expectations are not met.

## 4. Observability Enhancements

* **Live UI Integrations (TUI & Web):** 
    * Fully connecting the `StateBroadcaster` to the Terminal User Interface and the Web Server. This will involve implementing WebSockets or Server-Sent Events (SSE) to push real-time telemetry to the browser, allowing users to visually monitor container stress and degradation as it happens.

* **Integration with OpenTelemetry:**
    * Instead of just keeping the StateBroadcaster internal to TUI/Web server, allow CHAOS to export its events (e.g., "Fault Injected at 10:04", "Teardown Started") in the OpenTelemetry (OTLP) format. This allows users to see chaos events overlaid on their existing Grafana or Datadog dashboards.

* **Advanced Metric Extraction (ObservationNeeds):** 
    * Moving beyond basic pass/fail validation. The goal is to capture granular metrics (e.g., capturing the exact CPU spike during the "Garbage Collection Death Spiral" right before an `OOMKilled` event occurs) to provide developers with deep diagnostic insights.

## 5. Visual Chaos Configuration (GUI/TUI Test Builder)

Relying on users to manually write a manifest.json is error-prone. To make CHAOS accessible to QA teams and developers, the Web Server and TUI should be upgraded from simple monitoring dashboards into interactive test builders.

* **The Improvement:** Build a dynamic "Manifest Builder" in the Web UI (using forms or a drag-and-drop interface) and a step-by-step wizard in the TUI to configure, validate, and launch chaos experiments visually.

* **Live Target Discovery:** The UI should fetch the list of available targets directly from the C++ backend's IContainerEngine. Instead of typing a container ID, the user selects a running container from a live dropdown list.

* **Dynamic Schema Validation:** If a user selects a Memory Cap perturbation, the UI should automatically query the C++ backend for the host's maximum memory (getSystemInfo()) and restrict the input slider so the user cannot enter an invalid limit.

* **One-Click Execution & Feedback Loop:** Once the test is built visually, the UI sends the JSON payload to a new POST /api/run endpoint on your C++ Web Server. The UI immediately subscribes to the StateBroadcaster (via WebSockets), transitioning seamlessly from "Test Builder" mode to "Live Monitoring" mode in a single fluid motion.

## 6. In-Container Fault Injection (The PID 1 Advanced Wrapper)

While the current architecture brilliantly uses the Docker API to apply constraints from the outside (infrastructure level), injecting a custom PID 1 wrapper into the container opens the door to application-level chaos.

* **The Improvement:** Evolve the initial wrapper concept into a lightweight, statically compiled agent (written in C++ or Rust) that is injected into the container at startup. The orchestrator communicates with this wrapper via a mounted Unix socket or gRPC.

* **Targeted Process Killing:** Instead of killing the entire container via Docker, the wrapper can target specific child processes (e.g., killing a background worker thread while leaving the main web server alive) to test partial degradation.

* **Application-Level Resource Exhaustion:** The wrapper can maliciously allocate heap memory from inside the container or burn CPU cycles internally. This triggers the application's actual Garbage Collector or Out-Of-Memory (OOM) handler more authentically than an external cgroup limit.

* **System Call Interception (Syscall Faults):** By using ptrace or eBPF within the wrapper, you can intercept the application's system calls. You can simulate a full disk by making the write() syscall return an ENOSPC error, or simulate file corruption by returning garbage data on read(), all without touching the host's actual hard drive.
