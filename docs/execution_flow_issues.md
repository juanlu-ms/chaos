# Execution Flow Issues

> **Status key:** ✅ FIXED — resolved in current codebase. ❌ OPEN — still needs work.

## Status Summary

| # | Issue | Status |
|---|-------|--------|
| 1 | SignalHandlerGuard global state | ❌ OPEN |
| 2 | DockerClient::getStats() mutable state | ✅ FIXED |
| 3 | CgroupMetricsGatherer thread safety | ❌ OPEN |
| 4 | Server detached thread | ✅ FIXED |
| 5 | PerturbationEngine cancel/scheduleAllAsync race | ✅ FIXED |
| 6 | waitForTeardown mutex deadlock | ❌ OPEN |
| 7 | Log parsing duplication | ✅ FIXED |
| 8 | Validation results computed twice | ✅ FIXED |
| 9 | No async result/error abstraction | ✅ FIXED |
| 10 | CliParser duplicates web logic | ❌ OPEN |
| 11 | Magic numbers | ❌ OPEN |
| 12 | exec() unused output | ✅ FIXED |
| 13 | parseResponse after killContainer | ❌ OPEN |
| 14 | Format string in pullImage | ✅ FIXED |
| 15 | std::async in observe() | ❌ OPEN |
| 16 | TransparentStringHash duplication | ✅ FIXED |
| 17 | RunSession is a struct | ❌ OPEN |

---

## CRITICAL: Thread Safety & Data Races

### 1. `SignalHandlerGuard` uses process-level global state, NOT instance-level state ❌ **OPEN**

**Files:** `orchestrator/src/signals/SignalHandlerGuard.cpp:17-18`

`g_stop_source` and `g_signal_pipe[2]` are file-scope globals. If two `SignalHandlerGuard` objects exist (even sequentially), the second one will close/recreate the pipe, potentially leaving the first one with dangling FDs. The destructor closes `pipe_read_` and `pipe_write_` but also sets `g_signal_pipe[0] = -1`. If a second guard is created, the new pipe data goes to new FDs, but the static `signalHandler()` still writes to the global `g_signal_pipe[1]` which may now be `-1` (causing SIGPIPE or failure).

**Impact:** The self-pipe trick with global state fundamentally prevents multiple `SignalHandlerGuard` instances from coexisting. Yet `CliParser::runPerturbationsLoop()` creates one per call.

---

### 2. `DockerClient::getStats()` has mutable state that is NOT thread-safe ✅ **FIXED**

**Files:** `orchestrator/include/containers/internal/DockerClient.hpp:207-211`

`mutable prev_net_rx_`, `prev_net_tx_`, `prev_net_timestamp_`, `prev_net_valid_` were mutated in a `const` method (`getStats()`). If `getStats()` was called concurrently from multiple threads, these mutable fields were accessed without synchronization.

**Fix:** Removed `const` from `getStats()` and `mutable` from the four fields. Propagated the `const` removal through `IContainerEngine` and `ObservabilityEngine` up to the `observe()` method. The compiler now prevents concurrent non-const calls without explicit synchronization.

---

### 3. `CgroupMetricsGatherer` is NOT thread-safe ❌ **OPEN**

**Files:** `orchestrator/src/containers/internal/CgroupMetricsGatherer.hpp:57-59`

`prev_cpu_usec_`, `prev_cpu_time_`, `prev_valid_` are instance state updated in `getCpuUsagePercent()` with no locking. The web server's metrics worker thread calls this from a `std::jthread` — currently each run creates its own `CgroupMetricsGatherer` instance (local variable in the jthread lambda), so there's no shared-instance race in practice, but the class itself is not marked thread-safe and is fragile to reuse.

---

## CRITICAL: Detached Thread + Shared Mutable State (Web Server)

### 4. `std::thread([...]).detach()` in `Server::handleRun()` ✅ **FIXED**

**Files:** `orchestrator/src/interfaces/web/Server.cpp:433`

The thread that executes the chaos run was **detached** — there was no mechanism to ensure it completed before the `Server` object or its members were destroyed.

**Fix:** Replaced the detached `std::thread` with a `std::jthread` member (`run_thread_`). Before starting a new run, any previous run is joined. The `jthread` destructor automatically calls `request_stop()` + `join()`, preventing use-after-free when `Server` is destroyed while a run is in progress.

---

## CONCURRENCY: `PerturbationEngine` Race Conditions

### 5. `PerturbationEngine::cancel()` and `scheduleAllAsync()` race ✅ **FIXED**

**Files:** `orchestrator/src/perturbations/PerturbationEngine.cpp:19,48`

`cancel()` called `stop_source_.request_stop()` without holding `threads_mutex_`. `scheduleAllAsync()` reassigned `stop_source_` without synchronization with `cancel()`.

**Fix:** Moved `stop_source_` reassignment in `scheduleAllAsync()` inside the existing `lock_guard` and wrapped `stop_source_.request_stop()` in `cancel()` with a `lock_guard`. The token extraction (`stop_source_.get_token()`) also happens under the lock so it sees the correct stop source.

---

### 6. `PerturbationEngine::waitForTeardown()` moves threads out under `threads_mutex_` ❌ **OPEN**

**Files:** `orchestrator/src/perturbations/PerturbationEngine.cpp:55-63`

`waitForTeardown()` swaps `active_threads_` under the mutex then joins outside the lock. `scheduleAllAsync()` holds the mutex while emplacing new threads. If `waitForTeardown()` and `scheduleAllAsync()` are called concurrently (possible if a new run starts while a previous run is still draining), they both contend on `threads_mutex_`. Not a deadlock with the current code (the mutex is held briefly in both paths), but the design is fragile — the per-thread lambdas inside `scheduleAllAsync` also acquire `threads_mutex_` for their `cancel_cv_.wait_for`, creating nested lock acquisition patterns.

---

## DESIGN: Duplicated Business Logic

### 7. Log parsing is duplicated in two places ✅ **FIXED**

**Files:**
- `orchestrator/src/observability/ObservabilityEngine.cpp:67-78` (canonical implementation)
- `orchestrator/src/interfaces/web/JsonSerializer.hpp` (previously had its own copy)

`parseLogLines` was previously duplicated in `ObservabilityEngine` and `JsonSerializer`. It has been unified into `observability::parseLogLines()` declared in `ObservabilityEngine.hpp:61` and used consistently from `Server.cpp`, `ObservabilityEngine.cpp`, and tests.

---

### 8. Validation results are computed TWICE in web mode ✅ **FIXED**

**Files:** `orchestrator/src/interfaces/web/Server.cpp:395-406`

**Fix:** `buildRunResults()` now calls `validation::validate()` once and derives both the boolean pass/fail and detailed results from the same call. `continuous_failures` (from mid-run validation) are merged into the final result.

---

## DESIGN: Missing Abstractions

### 9. No async result/error abstraction ✅ **FIXED**

**Files:** `orchestrator/src/interfaces/web/Server.cpp:220-454`

**Fix:** Replaced the raw `std::thread` with `std::jthread` member (`run_thread_`) with RAII join. Error and result storage extracted into `storeRunResult()` and `storeRunError()` helper functions for consistent thread-safe access.

---

### 10. `CliParser::runPerturbationsLoop()` duplicates the web server's run logic ❌ **OPEN**

**Files:**
- `orchestrator/src/interfaces/cli/CliParser.cpp:236-278`
- `orchestrator/src/interfaces/web/Server.cpp:278-499`

Both implement a similar loop (observe, broadcast, wait, check cancellation) but with different mechanisms:
- CLI uses blocking `::read()` on signal pipe with a 500ms poll
- Web uses `condition_variable` on `RunSession`
- Both could share a common `RunOrchestrator` abstraction.

---

## CONFIGURATION: Hardcoded Values

### 11. Magic numbers scattered throughout the codebase ❌ **OPEN**

| Value | File | Line | Description |
|-------|------|------|-------------|
| `/sys/fs/cgroup/system.slice` | `CgroupMetricsGatherer.cpp` | 18 | CGROUP_ROOT hardcoded |
| `500` (ms) | `CliParser.cpp` | 267 | Signal loop poll interval |
| `100` (ms) | `Server.cpp` | 317 | Metrics worker poll interval |
| `1500` (ms) | `Server.cpp` | 318 | Logs worker poll interval |
| `2` (s) | `Server.cpp` | 319 | Baseline "normal" phase duration |
| `100000` | `CpuCapPerturbation.cpp` | 16 | kCpuPeriod hardcoded |
| `50` | `DockerClient.cpp` | 479 | tail=50 for logs |
| `eth0` | multiple | — | Default network iface in 2 places |
| `5` | `Server.cpp` | 369 | Continuous validation tick interval (every 5th poll = 500ms) |

---

## ERROR HANDLING

### 12. `DockerClient::exec()` unused output ✅ **FIXED**

**Files:**
- `orchestrator/src/perturbations/internal/NetworkDelayPerturbation.cpp:42,68`
- `orchestrator/src/perturbations/internal/NetworkCutoffPerturbation.cpp:33,85`
- `orchestrator/src/perturbations/internal/GarbagePacketPerturbation.cpp:59`

The network perturbations now use `engine_->execInNetNs()` (not the old `exec()`) and capture the return value (`const auto execOut`). The output is logged at DEBUG level when non-empty. `exec()` itself is no longer used for perturbations.

---

## MINOR ISSUES

### 13. `DockerClient::parseResponse()` called after `killContainer()` but response body may be empty ❌ **OPEN**

**Files:** `orchestrator/src/containers/internal/DockerClient.cpp:326`

The 204 response has no body, so `parseResponse()` would throw. The current code guards with `!response.body.empty()`, so the throw is avoided. But the parsed JSON is discarded — it's only called for validation side-effects. Not a correctness bug, but wasteful.

---

### 14. Format string vulnerability in `DockerClient::pullImage()` ✅ **FIXED**

**Files:** `orchestrator/src/containers/internal/DockerClient.cpp:216-217`

`fmt::format("Repository not found ({})", response.status)` — the status is now correctly interpolated. The error messages include the HTTP status code.

---

### 15. `ObservabilityEngine::observe()` uses `std::async(std::launch::async, ...)` for log fetching ❌ **OPEN**

**Files:** `orchestrator/src/observability/ObservabilityEngine.cpp:28`

`std::async(std::launch::async, [this, &containerId]() { return getLogs(containerId); })` forces a new thread per `observe()` call. This is wasteful for a polling loop. A thread pool or async I/O would be more appropriate.

---

### 16. `TransparentStringHash` is defined TWICE ✅ **FIXED**

**Files:**
- `orchestrator/include/manifests/Manifest.hpp:18-30` (canonical definition)
- `orchestrator/src/manifests/ManifestParser.cpp` (previously had its own)

`TransparentStringHash` is now defined once in `Manifest.hpp:18` and used via `using` declarations in `ManifestParser.cpp`. No duplication.

---

### 17. `RunSession` is a struct with all public members ❌ **OPEN**

**Files:** `orchestrator/include/core/RunSession.hpp:25-37`

No encapsulation, no invariant enforcement. Used as a raw shared-data blob with external mutex discipline. This is a deliberate trade-off for simplicity — the struct is accessed from multiple threads with disciplined mutex usage.
