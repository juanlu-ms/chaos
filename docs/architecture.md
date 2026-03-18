# Chaos Orchestrator Architecture

## Perturbations Engine Design

### Overview
The perturbation engine follows the **Factory Mode** and **Strategy** design patterns to decouple the instantiation and execution of chaos attacks from the core orchestrator.

### Interfaces and SOLID Compliance
All perturbations implement the `IPerturbation` interface. This ensures compliance with the **Open/Closed Principle (OCP)**: adding new attacks does not require modifying the orchestrator logic. It also honors **Dependency Inversion**, as the orchestrator depends on the abstract `IPerturbation` rather than concrete implementations (like `KillPerturbation`).

The `IPerturbation::apply` method takes a reference to the `IContainerEngine` ensuring strict object lifecycle management without the overhead of shared ownership (`std::shared_ptr`).

## Execution Methods
Perturbations employ OS-native structures:

### `cgroups` (Control Groups)
Used for CPU and Memory constraints (`CpuCapPerturbation`, `MemoryCapPerturbation`). We interface natively with Linux `/sys/fs/cgroup` (cgroup v2) bounds to assert memory limit restrictions and throttle CPU quotas without introducing proxy components that would increase latency or baseline memory consumption.

### `tc` (Traffic Control)
Used for Network constraints (`NetworkCapPerturbation`). Using Linux `iproute2` traffic control tools, we can inject bandwidth constraints and packet latency directly into the container's network namespace (e.g. `tc qdisc add dev eth0 root netem delay 100ms`).

## Error Handling
The engine relies exclusively on `<system_error>` (`std::system_error`) instead of generic runtime exceptions. When a lower-level subsystem (`cgroups`, `tc`, POSIX signals) fails, the underlying OS error code (`std::errc`) is preserved, allowing the orchestrator to log exact system-level semantics for diagnostic feedback.
