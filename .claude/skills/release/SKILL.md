---
name: release
description: Cut a CHAOS release. Runs pre-flight checks, then (only after explicit confirmation) runs scripts/bump-version.sh, which commits, tags vX.Y.Z and pushes, triggering the GitHub release build.
argument-hint: "[patch|minor|major]"
disable-model-invocation: true
---

# /release: pre-flight, confirm, bump

Bump level: `$ARGUMENTS` (default `patch`).

`scripts/bump-version.sh` is **not reversible from here**:
- It edits `CMakeLists.txt`, `vcpkg.json` and `frontend/package.json`.
- It commits "Bump version to X.Y.Z" and creates the tag `vX.Y.Z`.
- It runs `git push --follow-tags`. The pushed `v*` tag makes CI build and publish a GitHub release.

## 1. Pre-flight (read-only; stop at the first failure and report it)

1. `git status --porcelain` is empty, the branch is `main`, and `git fetch && git status -sb` shows it is not behind `origin/main`.
2. Work out the current version (`grep -oP 'VERSION \K[0-9.]+' CMakeLists.txt`) and the next one, and check that `vcpkg.json` and `frontend/package.json` carry the same current version.
   - The script replaces **every** occurrence of the old version string in those files with `sed s/old/new/g`. Grep each file for the old version and make sure every hit really is the project version.
3. `scripts/devc build`, `scripts/devc test-unit`, `scripts/devc check-format` and `scripts/devc frontend-test` all pass. Show the summary lines.
4. Docs that mention a version, such as the README download examples and `docs/USER_GUIDE.md`: list any that still show an older version. Report them; whether to fix them first is the user's call.
5. Summarise the commits since the last tag (`git log $(git describe --tags --abbrev=0)..HEAD --oneline`) as draft release notes.

## 2. Confirm

Show:
- Version `CURRENT → NEXT`.
- The draft notes.
- The stale-docs list.
- Exactly what will happen: commit, tag `vNEXT`, push to origin, and a public GitHub release.

Then **ask the user to confirm** and wait. Any answer other than an explicit yes means stop.

## 3. Run

Run `scripts/bump-version.sh <level>` and show its output. Afterwards, give the GitHub Actions run URL (`gh run list --limit 3`), so the user can watch the release build.
