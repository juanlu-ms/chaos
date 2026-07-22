# CHAOS Development Guide

## Prerequisites

- **Compiler:** Clang 18 (or GCC 14+ with `libstdc++-14`)
- **Build:** CMake 3.24+, Ninja
- **Package manager:** vcpkg in manifest mode — set `VCPKG_ROOT` to your vcpkg checkout (dependencies are declared in `vcpkg.json`)
- **Frontend (optional):** pnpm 9+, Node.js 24+

On Ubuntu 24.04:

```bash
sudo apt-get install clang-18 lld-18 libstdc++-14-dev ninja-build
```

## First-time Setup

Point `VCPKG_ROOT` at your vcpkg checkout; the CMake presets pick it up as the
toolchain file and install the manifest dependencies on first configure:

```bash
export VCPKG_ROOT=/path/to/vcpkg
```

## Building from Source

### Debug Build

```bash
cmake --preset dev-linux-clang
cmake --build --preset debug -- -j$(nproc)
```

Binary lands at `build/dev-linux-clang/orchestrator/chaos`.

### Release Build

```bash
cmake --preset release-linux-clang
cmake --build --preset release -- -j$(nproc)
```

### Frontend Embedding

When `CHAOS_EMBED_WEB_UI=ON` (default), CMake automatically runs `pnpm install && pnpm run build` during configure and embeds the static assets into the `chaos` binary via [CMakeRC](https://github.com/vector-of-bool/cmrc). A stamp file skips redundant rebuilds.

To force a frontend rebuild:

```bash
cmake --build --preset debug --target chaos_frontend
cmake --preset dev-linux-clang    # re-embed
```

To disable embedding entirely (e.g. on systems without pnpm):

```bash
cmake --preset dev-linux-clang -DCHAOS_EMBED_WEB_UI=OFF
```

During development, use `pnpm run dev` in `frontend/` for hot module reload at `http://localhost:5173`. The dev server proxies `/api` requests to the backend on `:8080`.

## Running Tests

| Layer | Command |
|-------|---------|
| C++ unit + integration | `ctest --test-dir build/dev-linux-clang --output-on-failure` |
| Frontend (Vitest) | `cd frontend && pnpm test` |
| E2E integration | `tests/e2e/` |

Alternate toolchain presets:

| Preset | Test command |
|--------|-------------|
| ASan/UBSan | `ctest --preset test-asan -L unit` |
| TSan | `ctest --preset test-tsan -L unit` |
| Coverage | `cmake --build build/ci-coverage --target coverage_report` |
| Release | `sudo ctest --preset test-release` |

## Code Formatting

```bash
cmake --workflow --preset check-format   # dry-run (CI gate)
cmake --workflow --preset format         # auto-fix in-place
```

## Repository Layout

```text
chaos/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── frontend/                   # Vite + pnpm Web UI project
│   ├── index.html
│   ├── package.json
│   ├── vite.config.js
│   ├── vitest.config.js
│   └── src/
│       ├── main.jsx
│       ├── style.css
│       ├── components/
│       ├── constants/
│       │   └── __tests__/
│       ├── hooks/
│       ├── steps/
│       └── utils/
│           └── __tests__/
├── orchestrator/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── containers/
│   │   ├── core/
│   │   ├── history/
│   │   ├── manifests/
│   │   ├── observability/
│   │   ├── perturbations/
│   │   ├── shared/
│   │   ├── signals/
│   │   ├── validation/
│   │   └── web/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── core/
│   │   ├── history/
│   │   ├── interfaces/
│   │   │   ├── cli/
│   │   │   └── web/
│   │   ├── containers/
│   │   │   └── internal/
│   │   ├── manifests/
│   │   ├── perturbations/
│   │   │   └── internal/
│   │   ├── observability/
│   │   │   └── internal/
│   │   ├── signals/
│   │   ├── validation/
│   │   │   └── internal/
│   └── tests/
│       ├── containers/
│       │   └── internal/
│       ├── core/
│       ├── history/
│       ├── interfaces/
│       │   └── cli/
│       ├── manifests/
│       ├── observability/
│       ├── perturbations/
│       ├── signals/
│       ├── smoke/
│       └── web/
├── wrapper/
│   ├── CMakeLists.txt
│   ├── src/
│   └── tests/
├── tests/
│   └── e2e/
└── docs/
```

## Testing Strategy

| Test suite | Location | Scope |
|------------|----------|-------|
| Docker adapter | `orchestrator/tests/containers/internal` | Container engine ownership |
| CLI parser | `orchestrator/tests/interfaces/cli` | CLI argument parsing |
| Manifest parsing | `orchestrator/tests/manifests` | JSON manifest parsing |
| Observability & validation | `orchestrator/tests/observability` | Metrics, validation engine, OTLP export |
| Perturbation engine & factory | `orchestrator/tests/perturbations` | Perturbation lifecycle, factory |
| ChaosRunner | `orchestrator/tests/core` | Build perturbations, validate, parse manifest |
| Run history | `orchestrator/tests/history` | Record serialization, file storage, recording |
| Signal handling | `orchestrator/tests/signals` | SignalHandlerGuard RAII, pipe, self-signal |
| JSON serialization | `orchestrator/tests/web` | State/limits JSON, log parsing |
| Smoke tests | `orchestrator/tests/smoke` | Public API smoke checks (no internal includes) |
| Frontend | `frontend/src/**/__tests__/` | Vitest unit tests for hooks, utils, constants |
| E2E integration | `tests/e2e` | Cross-component integration |

## Architecture

For a detailed breakdown of design decisions, concurrency models, and the Docker API Facade pattern, see [ARCHITECTURE.md](ARCHITECTURE.md).
