# Phase 0.7 Phase-Aware Animation Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the first phase-aware action flow to PetRuntime so one semantic action can be composed from enter/loop/exit GIF clips.

**Architecture:** Keep single-clip actions compatible, add optional `phases` to manifest actions, and let C++ PetRuntime advance phases while QML only reports frame completion. Use `sleep -> sleeping -> wake` as the first concrete flow.

**Tech Stack:** Qt 6, Qt Quick/QML, C++17, CMake, CTest, Python static checks.

---

## Files

- Create: `tests/check_phase_0_7_phase_runtime.py`
- Modify: `CMakeLists.txt`
- Modify: `apps/desktop/resources/pet_assets.qrc`
- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/qml/PetWindow.qml`
- Modify: `docs/v2/phase0-desktop-shell.md`

## Tasks

### Task 1: Red Test

- [ ] Add `tests/check_phase_0_7_phase_runtime.py` checking phase-aware manifest and runtime surface.
- [ ] Register it in `CMakeLists.txt`.
- [ ] Run `cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew`.
- [ ] Run `ctest --test-dir build -R check_phase_0_7_phase_runtime --output-on-failure`.
- [ ] Expected: fails because phase-aware action/runtime does not exist yet.

### Task 2: Manifest and Resources

- [ ] Add qrc aliases for `sleep`, `sleeping`, and `wake` left/right GIFs.
- [ ] Add `sleep` action with `enter`, `loop`, and `exit` phases.
- [ ] Keep current single-phase actions compatible.
- [ ] Run the Phase 0.7 test again.
- [ ] Expected: still fails because runtime code is not implemented.

### Task 3: Runtime

- [ ] Add `currentPhaseId` property.
- [ ] Add `testSleep()` invokable.
- [ ] Parse phase definitions from manifest.
- [ ] Implement `playPhase(actionId, phaseId)`.
- [ ] Let `handleAnimationFinished()` advance `nextPhase` or return idle.
- [ ] Let `returnToIdle()` play `exitPhase` when available.
- [ ] Run `ctest --test-dir build -R check_phase_0_7_phase_runtime --output-on-failure`.
- [ ] Expected: Phase 0.7 test passes.

### Task 4: QML and Verification

- [ ] Add “测试睡觉” menu item.
- [ ] Notify `handleAnimationFinished()` for `once` as well as `onceThenIdle`.
- [ ] Run:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
/opt/homebrew/bin/qmllint -I build/apps/desktop apps/desktop/qml/PetWindow.qml
git diff --check
```

- [ ] Manually test sleep -> sleeping -> wake -> idle.

### Task 5: Publish

- [ ] Update PR #10 description.
- [ ] Commit as `feat: add phase-aware animation flow`.
- [ ] Push `v2-ai-pet`.

## Self-Review

- Scope is narrow: only one phase-aware sample action, no local frame slicing.
- Single-phase actions stay compatible.
- The runtime remains intentionally small and does not implement queues or priorities yet.
