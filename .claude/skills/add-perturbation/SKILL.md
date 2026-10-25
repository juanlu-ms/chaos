---
name: add-perturbation
description: Checklist for adding or changing a CHAOS perturbation or expectation type, or one of their parameters. That touches the C++ class, factory, parser, CMake, tests, docs, frontend constants and examples together. Use whenever a manifest type or parameter is added, renamed or changed.
argument-hint: <type name and what it does>
---

# Adding or changing a perturbation or expectation type

Request: $ARGUMENTS

A manifest type is a **contract** shared by about 8 places. Missing one ships a feature the UI can't configure, docs that lie, or an example that fails. The list below comes from real history (e.g. commit `5992db8`, which renamed GarbagePacket → TrafficCorruption), not from guesswork.

Size: this is at least an **M** task in the `/build` sense, and an **L** task if the fault needs new privileged mechanics (new tc/iptables/cgroup usage).

## Perturbation: the touch points, in order

1. **Design first.** Before writing code, settle three things:
   - What gets applied, and with which Linux mechanism.
   - What revert must restore. Restore the *previous* state, not a default.
   - Which params exist, with their types, units, ranges and defaults.
   Write these down in the plan.
2. **Unit test first** in `orchestrator/tests/perturbations/<Name>PerturbationUnitTest.cpp`.
   - Use `MockContainerEngine.hpp` to assert the exact commands and calls for apply, revert, partial-apply rollback, and invalid params.
   - Add a factory test (`FactoryCreates<Name>Perturbation`) in `orchestrator/tests/perturbations/PerturbationTests.cpp`, next to the existing ones.
3. **Implementation** in `orchestrator/src/perturbations/internal/<Name>Perturbation.{hpp,cpp}`, implementing `IPerturbation` (`orchestrator/include/perturbations/IPerturbation.hpp`). Follow the closest existing sibling:
   - network → `NetworkDelayPerturbation` / `NetworkCutoffPerturbation`
   - resources → `CpuCapPerturbation` / `MemoryCapPerturbation`
   - process → `KillPerturbation`
   - Keep a rollback list for partial applies, as `NetworkCutoffPerturbation::revertCommands_` does.
   - **Validate every param's type and range before it reaches any command string.** Commands go through `execInNetNs` → `popen` (a shell).
4. **Factory:** add the `type` string to `orchestrator/src/perturbations/PerturbationFactory.cpp`.
5. **Parser:** add the required params to `kPerturbationRequiredParams` in `orchestrator/src/manifests/ManifestParser.cpp`, plus a case in `orchestrator/tests/manifests/ManifestParserUnitTest.cpp`.
6. **Build:** add the `.cpp` to the source list in `orchestrator/CMakeLists.txt` (next to the other `src/perturbations/internal/*.cpp`).
7. **Docs:**
   - Add a section to `docs/Perturbations.md` (params table with units, defaults and an example).
   - Update the type count and list in `README.md` ("7 perturbation types"), and in `docs/USER_GUIDE.md` wherever types are listed.
   - Add a line in `docs/ROADMAP.md` if the type was listed there as planned.
8. **Frontend:** add an entry to `PERTURBATION_PARAMS` in `frontend/src/constants/perturbations.js` (label, required/optional, placeholder), and extend `frontend/src/constants/__tests__/perturbations.test.js`.
9. **Example:** add `examples/demo/NN-<name>.json`, using the demo target's real port.
10. **E2E** (optional, needs root and Docker): add a case to `tests/e2e/PerturbationE2eTest.cpp`. **Write it, but don't run it.** Ask the user to run it.

## Expectation: the touch points

The same pattern applies, with these differences:
- Implementation: `orchestrator/src/validation/internal/`.
- Factory: `orchestrator/src/validation/ValidationFactory.cpp`.
- Parser rules: the expectation rule map in `ManifestParser.cpp`.
- Tests: `orchestrator/tests/validation/`.
- Frontend: `frontend/src/constants/expectations.js`.
- Docs: the expectations section of `docs/Perturbations.md`.

## Verify

- `scripts/devc build && scripts/devc test-unit && scripts/devc check-format && scripts/devc frontend-test`
- Then dispatch, in parallel:
  - `manifest-contract-checker` on the diff. It must report no drift.
  - `linux-faults-reviewer` for perturbations.
  - `cpp:cpp-reviewer`.
