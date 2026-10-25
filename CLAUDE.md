# CHAOS

C++23 tool (with a React web UI) that injects faults into Docker containers (cpu/memory caps, kill, tc/netem, iptables, packet floods) and validates recovery against expectations. **It runs as root and changes host networking.** Architecture: `docs/ARCHITECTURE.md` (partly outdated: trust the code). Dev guide: `docs/DEVELOPMENT.md`.

## Environment and commands

The canonical toolchain is the **devcontainer** (clang 18, clang-format 18, vcpkg, the same as CI). The host has clang 23, whose formatting differs from CI. Run every check through `scripts/devc` from the host. It finds the container, starts it if stopped, and maps host paths to `/workspaces/chaos`.

| Task | Command |
|---|---|
| Build (debug) | `scripts/devc build` |
| Unit tests | `scripts/devc test-unit` (one test: `scripts/devc test-unit -R <regex>`) |
| Format / CI format gate | `scripts/devc format` / `scripts/devc check-format` |
| Frontend tests (Vitest) | `scripts/devc frontend-test` |
| Sanitizers | `scripts/devc exec bash -lc "cmake --preset ci-tsan && cmake --build --preset debug-tsan && ctest --preset test-tsan -L unit"` (same for `ci-asan-ubsan` / `debug-asan` / `test-asan`) |

A PostToolUse hook formats each edited C++ file with clang-format 18 through the container. If it reports that it skipped a file, run `scripts/devc format` before finishing.

## Never run without asking first

- `smoke`/`e2e` tests, `ctest --preset test-release`, `sudo ...`, and `chaos run`/`chaos serve`. They need root and Docker, and they change real containers and host networking. The devcontainer is `--privileged --pid=host`, so `scripts/devc exec` counts as root on the host.
- `scripts/bump-version.sh`: it commits, tags **and pushes**, which publishes a GitHub release. Use the `/release` skill.
- Default to unit tests (`-L unit`). They use `MockContainerEngine` and touch nothing real.

## Contracts that must change together

A perturbation or expectation type, or one of its parameters, lives in: the C++ class + `PerturbationFactory`/`ValidationFactory` + the `ManifestParser` required-params map + `orchestrator/CMakeLists.txt` + unit tests + `docs/Perturbations.md` + `frontend/src/constants/{perturbations,expectations}.js` + `examples/`. Use the `add-perturbation` skill, and finish with the `manifest-contract-checker` agent.

Manifest params go through `IContainerEngine::execInNetNs` → `popen` (a shell). Validate the type and range of every param before it can reach a command string.

## Conventions

- Commit subjects: an imperative sentence in English ("Add X", "Fix Y when Z"). **No** conventional-commit prefixes, even if a commit skill suggests them.
- Don't commit or push unless asked. Branch off `main` for multi-commit work.
- Comments explain *why*, not *what* (that's the existing style).

## How we work

Pick the mode from the request:
- **I wrote code, review it** → `/dev-core:review-me`. Teach; never edit my code unless I ask.
- **Implement this** → `/dev-core:build`. Size it S/M/L first. Only L tasks use the full superpowers flow (brainstorm → spec → plan → TDD). S/M tasks skip that ceremony even when a superpowers skill says it applies.
- **Explain X** → `/dev-core:explain`.
- After a correction or a surprise → suggest `/dev-core:learn`.

Teaching: I'm a junior engineer, comfortable with C++23, Linux, concurrency and cgroups. Explain decisions and any concept I may not know (in 1–2 lines, with where to read more). If my approach is bad, say so plainly. When a decision has no single right answer, give me the options and let me choose.

## Reviewers

`/build` and `/review-me` dispatch these by path (`dev-core:code-reviewer` always runs):
- `orchestrator/src/perturbations/**`, `orchestrator/src/containers/**`, `orchestrator/src/signals/**`, any shell-command building → `linux-faults-reviewer`, `cpp:cpp-reviewer`
- `orchestrator/**/*.{cpp,hpp}` → `cpp:cpp-reviewer`. Add `cpp:concurrency-reviewer` if the diff touches threads, jthread/stop_token, mutexes, atomics, signals, `PerturbationEngine`, `SharedState`, or the web server's SSE.
- `frontend/src/**` → `frontend-reviewer`
- `docs/Perturbations.md`, `examples/**`, `ManifestParser.cpp`, `frontend/src/constants/**`, any factory → `manifest-contract-checker`
- Behaviour changes or test changes → `dev-core:test-reviewer`
