# Phase 0.6 Animation Runtime Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the Phase 0.5 PetRuntime into a small animation runtime skeleton that understands action metadata, facing variants, playback serials, and one-shot auto-return behavior.

**Architecture:** Keep animation policy in C++ `PetRuntime`, keep QML as a thin rendering layer around `AnimatedImage`, and describe skin/action data in the embedded manifest. This phase adds static tests first, then implements only the minimum behavior needed for action/facing selection and one-shot completion.

**Tech Stack:** Qt 6, Qt Quick/QML, C++17, CMake, CTest, Python static checks.

---

## Files

- Create: `docs/v2/animation-assets-inventory.md`
- Create: `tests/check_phase_0_6_animation_runtime.py`
- Modify: `CMakeLists.txt`
- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Modify: `apps/desktop/resources/pet_assets.qrc`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/qml/PetWindow.qml`
- Modify: `docs/v2/phase0-desktop-shell.md`

---

### Task 1: Write the Failing Static Test

- [ ] Add `tests/check_phase_0_6_animation_runtime.py`.
- [ ] Register it in root `CMakeLists.txt`.
- [ ] Run `cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew`.
- [ ] Run `ctest --test-dir build -R check_phase_0_6_animation_runtime --output-on-failure`.
- [ ] Expected result: the test fails because manifest v2, facing properties, playback serial, and QML completion hook do not exist yet.

### Task 2: Add Inventory and Manifest v2

- [ ] Create `docs/v2/animation-assets-inventory.md` with initial mappings for stand, walk, run, once, and special GIFs.
- [ ] Upgrade `apps/desktop/resources/skins/miles-edgeworth/manifest.json` to `schemaVersion: 2`.
- [ ] Add action variants for right/left facing.
- [ ] Add qrc aliases for bow and tea action resources.
- [ ] Run `ctest --test-dir build -R check_phase_0_6_animation_runtime --output-on-failure`.
- [ ] Expected result: test still fails because runtime code is not implemented yet.

### Task 3: Extend PetRuntime

- [ ] Add QML properties: `currentFacing`, `currentLoopMode`, `currentAutoReturnToIdle`, `playbackSerial`.
- [ ] Add invokable methods: `setFacing`, `toggleFacing`, `testObjecting`, `testBow`, `handleAnimationFinished`.
- [ ] Parse manifest v2 `variants` and choose animation by current facing.
- [ ] Increment `playbackSerial` whenever an action is played, including replaying the same action.
- [ ] Return to idle only when `handleAnimationFinished()` is called for an action whose `loopMode` is `onceThenIdle`.
- [ ] Run `ctest --test-dir build -R check_phase_0_6_animation_runtime --output-on-failure`.
- [ ] Expected result: Phase 0.6 static test passes.

### Task 4: Update QML Rendering Hooks

- [ ] Add menu items for `toggleFacing()`, `testObjecting()`, and `testBow()`.
- [ ] Add an `onCurrentFrameChanged` hook that calls `App.PetRuntime.handleAnimationFinished()` only when `frameCount > 0` and the last frame is reached.
- [ ] Add a `Connections` block for `playbackSerialChanged` to reset and restart the `AnimatedImage`.
- [ ] Run `/opt/homebrew/bin/qmllint -I build/apps/desktop apps/desktop/qml/PetWindow.qml`.
- [ ] Expected result: no QML lint warnings.

### Task 5: Verify and Publish

- [ ] Run `cmake --build build`.
- [ ] Run `ctest --test-dir build --output-on-failure`.
- [ ] Run `/opt/homebrew/bin/qmllint -I build/apps/desktop apps/desktop/qml/PetWindow.qml`.
- [ ] Launch the app and manually test right-click menu actions.
- [ ] Update PR #10 description with Phase 0.6 progress and verification.
- [ ] Commit as `feat: add animation runtime skeleton`.
- [ ] Push `v2-ai-pet`.

## Self-Review

- Spec coverage: The plan covers manifest schema, resource inventory, runtime facing selection, one-shot completion, tests, docs, and PR update.
- Placeholder scan: No unresolved placeholder instructions remain.
- Type consistency: Runtime property names match the design: `currentFacing`, `currentLoopMode`, `currentAutoReturnToIdle`, and `playbackSerial`.
