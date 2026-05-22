# Phase 2.3.1 动画-文字同步状态机与速率限制器 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make streaming text emerge at a human pace and gate on animation transitions, by landing the full ChatController state machine, a character rate limiter (ChatTextPacer) that consumes the `msPerChar` setting from Phase 2.2, and the `requestBoundaryAndNotify` / `requestCleanFinishAndNotify` notify API on PetRuntime — all per `docs/v2/设计方案/AI 聊天动画编排设计.md` §5–7.

**Architecture:** ChatController gains an explicit 5-state machine (`IDLE` / `BUFFERING_FOR_START` / `STREAMING` / `GATED` / `WAITING_FOR_ANIMATION_END`) that decides whether streamed text is routed straight to the pacer or buffered in `m_holdBuffer` waiting for an animation boundary. A new `ChatTextPacer` (QObject + QTimer) emits one grapheme per tick at `msPerChar` rate, with a backlog catch-up that halves the interval when the queue exceeds 30 chars. PetRuntime grows two callback-based methods that fire at the next animation boundary (Phase 2.3.1) and will play `exit` phases when Phase 2.4's phased animations land (API stable, behavior upgrades). The pacer is owned by ChatController; PetRuntime stays unaware of chat state.

**Tech Stack:** Qt 6 (C++17 + `QObject` + `QTimer` + `std::function<void()>` callbacks), CMake/Ninja/CTest, Python 3 for the static contract check. No new third-party dependencies.

---

## Scope Check

This plan implements **Phase 2.3.1: 动画-文字同步** only. Phase 2.3 in the rough plan also covers persona content, session history (SQLite), and Langfuse — those are **separate plans** (Phase 2.3.2 and 2.3.3) and explicitly out of scope here.

Included:
- `ChatTextPacer` class with append, msPerChar setter, backlog catch-up, `chunkReady` signal.
- ChatController state machine (5 states + transitions per AI 聊天动画编排设计 §5).
- ChatController integration with the pacer (replaces immediate `flushHoldBuffer` write path).
- ChatController GATED 800ms safety timeout.
- ChatController cancel / error / network-failure path (drain hold buffer to pacer, return to IDLE, call `requestCleanFinish`).
- PetRuntime `requestBoundaryAndNotify(std::function<void()>)` and `requestCleanFinishAndNotify(std::function<void()>)` with 1500ms safety timeout. Both methods behave identically in Phase 2.3.1 (boundary semantic); Phase 2.4 upgrades `requestCleanFinishAndNotify` to play `exit` phase without changing the protocol.
- Tests: `chat_text_pacer_smoke` (new), extended `chat_controller_smoke` (state transitions), extended `pet_runtime_smoke` (boundary callbacks), Python contract check.

Excluded:
- Full Miles persona prompt content — Phase 2.3.2.
- SQLite session history / new-or-clear conversation / history truncation — Phase 2.3.2.
- Langfuse observability — Phase 2.3.3.
- Phased `enter` / `loop` / `exit` animation support — Phase 2.4 (PetRuntime API ready, but no `exit` segment is played in 2.3.1).
- Manifest JSON Schema validation — Phase 2.4.
- Persona / SettingsController surface in QML — no UI changes in 2.3.1.

## Assumptions

- Execution starts on `main` (or a fresh worktree branched from `main`). Phase 2.2 is shipped: `SettingsService::msPerChar()` returns a persisted 40–200 ms value, the `chat_controller_smoke` test already constructs `ChatController(&runtime, &settings)`.
- Baseline contract checks (`check_phase_2_0_ai_chat_mvp`, `check_phase_2_1_provider`, `check_phase_2_2_settings`) and all C++ smoke tests pass on `main`.
- Qt 6.5+ available, Apple-Silicon Homebrew under `/opt/homebrew`.
- The current Miles skin's `thinking` / `speaking` / `idle` actions are simple `loop` or `oneshot` actions (no multi-phase `enter` / `loop` / `exit`). Phase 2.4 will introduce phased actions; the API designed here must not require revision when that lands.

## Required Reading

Before implementing, read:
- `docs/v2/设计方案/AI 聊天动画编排设计.md` §3 (model output convention), §4 (SSE event stream), §5 (ChatController state machine — the canonical source for transition behavior), §6 (PetRuntime API contract + boundary table), §7 (rate limiter). All design questions during implementation should be answered by consulting this document, **not** by inventing behavior.
- Current `apps/desktop/src/chat/ChatController.{h,cpp}` to understand the existing `m_holdBuffer` plumbing from Phase 2.1.
- Current `apps/desktop/src/pet/PetRuntime.{h,cpp}::handleAnimationFinished` to see where boundary callbacks will hook in.

## File Structure

Create:
- `apps/desktop/src/chat/ChatTextPacer.h`
  - `ChatTextPacer` `QObject` with `append(const QString &)`, `setMsPerChar(int)`, `msPerChar() const`, `pendingCount() const`, signal `chunkReady(const QString &)`. Internal: `QTimer`, character queue, backlog speedup state.
- `apps/desktop/src/chat/ChatTextPacer.cpp`
  - Timer-driven emit, surrogate-pair-aware character pop, backlog catch-up logic.
- `apps/desktop/tests/chat_text_pacer_smoke.cpp`
  - Round-trip test that uses an event loop to verify emitted chunks under controlled timing.
- `tests/check_phase_2_3_1_animation_sync.py`
  - Static contract check.
- `docs/v2/阶段记录/Phase 2.3.1 动画-文字同步.md`
  - Stage record (final task only).

Modify:
- `apps/desktop/src/chat/ChatController.h`
  - Add `enum class ChatPhase`, member `m_phase`, pending-expression members, `ChatTextPacer *m_pacer`, gate-timeout `QTimer m_gateTimeout`, new private methods (`transitionTo`, `handleCleanFinishReady`, `handleBoundaryReached`, `handleGateTimeout`, `drainHoldBufferToPacer`, `appendChunkToCurrentMessage`).
- `apps/desktop/src/chat/ChatController.cpp`
  - Construct pacer + connect signals, install gate timer, rewrite `applyStreamEvent` to drive transitions, replace `flushHoldBuffer` immediate write with pacer-routed `appendChunkToCurrentMessage`, drain buffer to pacer on cancel/error.
- `apps/desktop/src/pet/PetRuntime.h`
  - Add `requestBoundaryAndNotify(std::function<void()>)` and `requestCleanFinishAndNotify(std::function<void()>)` declarations, a small private struct + `QList` for pending notifications, `drainPendingNotifications()`.
- `apps/desktop/src/pet/PetRuntime.cpp`
  - Implement the two notify methods, hook `drainPendingNotifications()` into `handleAnimationFinished` and `setCurrentAction`, install 1500ms safety timer per notification.
- `apps/desktop/tests/chat_controller_smoke.cpp`
  - Add new assertions for: BUFFERING_FOR_START → STREAMING via simulated `handleAnimationFinished`, mid-stream expression switch (STREAMING → GATED → STREAMING), RUN_FINISHED → WAITING_FOR_ANIMATION_END → IDLE, cancel drains hold buffer to pacer.
- `apps/desktop/tests/pet_runtime_smoke.cpp`
  - Add a block that calls `requestBoundaryAndNotify` and asserts the callback fires after `handleAnimationFinished`.
- `apps/desktop/CMakeLists.txt`
  - Add `ChatTextPacer.{h,cpp}` to `DESKTOP_SOURCES`, register a new `ChatTextPacerSmoke` test target with `Qt6::Core` + `Qt6::Test` (for `QSignalSpy`).
- `CMakeLists.txt`
  - Register `check_phase_2_3_1_animation_sync` ctest.
- `docs/v2/文档索引.md`
  - Link the Phase 2.3.1 stage record (final task).
- `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md`
  - Add a "已完成" callout under Phase 2.3 noting that 2.3.1 (动画-文字同步部分) is done; 2.3.2 (persona/history) and 2.3.3 (Langfuse) remain (final task).

Not modified (intentional):
- `apps/agent-core/**` — sidecar already emits the correct event sequence per Phase 2.1; no change needed for 2.3.1.
- `apps/desktop/qml/ChatWindow.qml` — UI consumes `messages` model unchanged; chunks emitted by the pacer flow into `messagesChanged` exactly as before.
- `apps/desktop/src/settings/**` — `msPerChar` is already persisted; ChatController reads it via `m_settings`.

---

## Task 0: Toolchain Preflight

**Files:**
- Read-only.

- [ ] **Step 1: Verify branch state**

Run:

```bash
git status --short --branch
```

Expected: clean working tree on `main` or a fresh worktree. If continuing from `phase-2-2-settings`, switch to `main` first via `git checkout main && git pull`. If a Phase 2.3.1 worktree is preferred, create one now using the `superpowers:using-git-worktrees` skill before continuing.

- [ ] **Step 2: Verify baseline tests pass**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew && \
  ctest --test-dir build -R "check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider|check_phase_2_2_settings|chat_controller_smoke|pet_runtime_smoke|settings_service_smoke" --output-on-failure
```

Expected: all listed tests pass. If any fail, stop and fix — Phase 2.3.1 builds on top of them.

- [ ] **Step 3: Verify the design doc is the version this plan targets**

Run:

```bash
grep -c "BUFFERING_FOR_START" "docs/v2/设计方案/AI 聊天动画编排设计.md"
```

Expected: at least 2. The design doc must contain the 5-state vocabulary (§5.1). If it doesn't, the design doc has drifted and this plan should be reconciled before implementation begins.

- [ ] **Step 4: No commit**

This task changes nothing.

---

## Task 1: Phase 2.3.1 Static Contract Scaffold

**Files:**
- Create: `tests/check_phase_2_3_1_animation_sync.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the contract test**

Create `tests/check_phase_2_3_1_animation_sync.py`:

```python
#!/usr/bin/env python3
"""Check the Phase 2.3.1 animation-text sync contract."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    file_path = ROOT / path
    if not file_path.exists():
        raise AssertionError(f"missing file: {path}")
    return file_path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    pacer_h = read("apps/desktop/src/chat/ChatTextPacer.h")
    pacer_cpp = read("apps/desktop/src/chat/ChatTextPacer.cpp")
    pacer_smoke = read("apps/desktop/tests/chat_text_pacer_smoke.cpp")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    controller_smoke = read("apps/desktop/tests/chat_controller_smoke.cpp")
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    runtime_smoke = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")
    stage_doc = read("docs/v2/阶段记录/Phase 2.3.1 动画-文字同步.md")
    index_doc = read("docs/v2/文档索引.md")

    # ChatTextPacer
    require("class ChatTextPacer" in pacer_h, "ChatTextPacer class must exist")
    require("void append(" in pacer_h, "ChatTextPacer must expose append()")
    require("setMsPerChar" in pacer_h, "ChatTextPacer must expose setMsPerChar()")
    require("msPerChar()" in pacer_h, "ChatTextPacer must expose msPerChar() getter")
    require("pendingCount()" in pacer_h, "ChatTextPacer must expose pendingCount() for tests")
    require("void chunkReady" in pacer_h, "ChatTextPacer must declare chunkReady signal")
    require("QTimer" in pacer_cpp, "ChatTextPacer must drive emission with QTimer")
    require("maxBacklog" in pacer_cpp.lower() or "backlog" in pacer_cpp.lower(),
            "ChatTextPacer must implement backlog catch-up")

    # ChatController state machine
    require("enum class ChatPhase" in controller_h, "ChatController must declare ChatPhase enum")
    require("IDLE" in controller_h and "BUFFERING_FOR_START" in controller_h
            and "STREAMING" in controller_h and "GATED" in controller_h
            and "WAITING_FOR_ANIMATION_END" in controller_h,
            "ChatPhase must include all 5 design states")
    require("ChatTextPacer" in controller_h, "ChatController must own a ChatTextPacer")
    require("transitionTo" in controller_h, "ChatController must expose a transitionTo helper")
    require("handleCleanFinishReady" in controller_h,
            "ChatController must handle PetRuntime cleanFinishReady callback")
    require("handleBoundaryReached" in controller_h,
            "ChatController must handle PetRuntime boundaryReached callback")
    require("handleGateTimeout" in controller_h,
            "ChatController must handle GATED safety timeout")
    require("drainHoldBufferToPacer" in controller_h,
            "ChatController must drain its hold buffer through the pacer")
    require("appendChunkToCurrentMessage" in controller_h,
            "ChatController must append pacer chunks via a dedicated method")
    require("requestBoundaryAndNotify" in controller_cpp,
            "ChatController must call requestBoundaryAndNotify on PetRuntime")
    require("requestCleanFinishAndNotify" in controller_cpp,
            "ChatController must call requestCleanFinishAndNotify on PetRuntime")

    # PetRuntime API
    require("requestBoundaryAndNotify" in runtime_h,
            "PetRuntime must declare requestBoundaryAndNotify")
    require("requestCleanFinishAndNotify" in runtime_h,
            "PetRuntime must declare requestCleanFinishAndNotify")
    require("std::function" in runtime_h,
            "PetRuntime notify API must take std::function callbacks")
    require("drainPendingNotifications" in runtime_cpp,
            "PetRuntime must drain pending notifications at animation boundaries")

    # Tests
    require("ChatTextPacer" in pacer_smoke,
            "pacer smoke test must exercise ChatTextPacer")
    require("msPerChar" in pacer_smoke,
            "pacer smoke test must cover msPerChar")
    require("backlog" in pacer_smoke.lower(),
            "pacer smoke test must cover backlog catch-up")
    require("BUFFERING_FOR_START" in controller_smoke
            or "ChatPhase" in controller_smoke,
            "controller smoke test must exercise the state machine")
    require("GATED" in controller_smoke
            or "expression switch" in controller_smoke.lower(),
            "controller smoke test must exercise mid-run expression gating")
    require("requestBoundaryAndNotify" in runtime_smoke,
            "runtime smoke test must exercise requestBoundaryAndNotify")

    # CMake
    require("ChatTextPacer.cpp" in desktop_cmake,
            "desktop CMake must list ChatTextPacer.cpp")
    require("ChatTextPacerSmoke" in desktop_cmake,
            "desktop CMake must register the ChatTextPacerSmoke target")
    require("chat_text_pacer_smoke" in desktop_cmake,
            "desktop CMake must register chat_text_pacer_smoke ctest")
    require("check_phase_2_3_1_animation_sync" in root_cmake,
            "root CMake must register the Phase 2.3.1 contract check")

    # Docs
    require("Phase 2.3.1" in stage_doc, "Phase 2.3.1 stage record must exist")
    require("Phase 2.3.1" in index_doc, "doc index must link Phase 2.3.1 record")

    print("phase 2.3.1 animation-text sync contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Run to verify it fails**

Run:

```bash
python3 tests/check_phase_2_3_1_animation_sync.py
```

Expected: `AssertionError: missing file: apps/desktop/src/chat/ChatTextPacer.h`. The check fails fast on the first missing artifact.

- [ ] **Step 3: Register in root CMakeLists.txt**

Open `CMakeLists.txt`. Find the existing Phase 2.2 block:

```cmake
        # Phase 2.2 的用户配置与安全存储检查：守住设置面板、Keychain
        # 路由、msPerChar 持久化以及 ChatController QProcess env 注入。
        add_test(
            NAME check_phase_2_2_settings
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_2_2_settings.py
        )
```

Append immediately after it (still inside the `if(BUILD_TESTING)` / `if(Python3_Interpreter_FOUND)` blocks):

```cmake
        # Phase 2.3.1 的动画-文字同步检查：守住 ChatController 状态机、
        # 字符速率限制器和 PetRuntime 的边界通知 API。
        add_test(
            NAME check_phase_2_3_1_animation_sync
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_2_3_1_animation_sync.py
        )
```

- [ ] **Step 4: Verify CTest sees the new test**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew >/dev/null && \
  ctest --test-dir build -N -R check_phase_2_3_1_animation_sync
```

Expected output contains `Test #N: check_phase_2_3_1_animation_sync`.

- [ ] **Step 5: Commit**

```bash
git add tests/check_phase_2_3_1_animation_sync.py CMakeLists.txt
git commit -m "test(phase-2-3-1): add animation-text sync contract scaffold (red)"
```

---

## Task 2: ChatTextPacer — Basic Stream (TDD)

**Files:**
- Create: `apps/desktop/src/chat/ChatTextPacer.h`
- Create: `apps/desktop/src/chat/ChatTextPacer.cpp`
- Create: `apps/desktop/tests/chat_text_pacer_smoke.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Write the failing smoke test (basic stream)**

Create `apps/desktop/tests/chat_text_pacer_smoke.cpp`:

```cpp
#include "chat/ChatTextPacer.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QString>
#include <QTimer>

#include <cassert>

namespace {

// Spin a Qt event loop until `predicate()` is true or `timeoutMs` elapses.
bool waitFor(QCoreApplication &app, int timeoutMs, auto predicate)
{
    QTimer deadline;
    deadline.setSingleShot(true);
    deadline.start(timeoutMs);
    while (!predicate() && deadline.isActive()) {
        app.processEvents(QEventLoop::AllEvents, 5);
    }
    return predicate();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // --- Basic stream: characters emerge one-per-tick at msPerChar interval ---
    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(20); // small interval keeps the test fast
        QSignalSpy chunks(&pacer, &ChatTextPacer::chunkReady);

        pacer.append(QStringLiteral("abc"));
        assert(pacer.pendingCount() == 3);

        const bool drained = waitFor(app, 1000, [&]() { return pacer.pendingCount() == 0; });
        assert(drained);

        QString assembled;
        for (const QList<QVariant> &args : chunks) {
            assembled.append(args.at(0).toString());
        }
        assert(assembled == QStringLiteral("abc"));
    }

    // --- CJK characters: one QChar per CJK glyph still works ---
    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(15);
        QSignalSpy chunks(&pacer, &ChatTextPacer::chunkReady);
        pacer.append(QStringLiteral("异议"));
        const bool drained = waitFor(app, 1000, [&]() { return pacer.pendingCount() == 0; });
        assert(drained);
        QString assembled;
        for (const QList<QVariant> &args : chunks) {
            assembled.append(args.at(0).toString());
        }
        assert(assembled == QStringLiteral("异议"));
        assert(chunks.size() == 2);
    }

    // --- msPerChar getter round-trip ---
    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(120);
        assert(pacer.msPerChar() == 120);
    }

    return 0;
}
```

- [ ] **Step 2: Write the ChatTextPacer header**

Create `apps/desktop/src/chat/ChatTextPacer.h`:

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

// ChatTextPacer drives streamed text into the UI at a human reading rate
// (default 80 ms / char). See `docs/v2/设计方案/AI 聊天动画编排设计.md` §7.
//
// Responsibility:
//   - Accept incoming text via append().
//   - Emit chunkReady(QString) on a QTimer at msPerChar intervals.
//   - Speed up when backlog exceeds maxBacklog (§7.3).
//
// What it is NOT:
//   - Not aware of ChatController state machine or PetRuntime.
//   - Not aware of [EXPR:tag] markers (sidecar strips those before SSE).
class ChatTextPacer : public QObject
{
    Q_OBJECT

public:
    explicit ChatTextPacer(QObject *parent = nullptr);

    // Queue text to be emitted one char per tick. Surrogate pairs are emitted
    // as a single chunk so emoji never split across ticks.
    void append(const QString &text);

    int msPerChar() const { return m_msPerChar; }
    void setMsPerChar(int value);

    // For tests: how many QChar units are still queued.
    int pendingCount() const { return m_queue.size(); }

signals:
    void chunkReady(const QString &chunk);

private:
    void tick();
    int effectiveInterval() const;

    QString m_queue;
    QTimer m_timer;
    int m_msPerChar = 80;

    static constexpr int kMaxBacklog = 30;
    static constexpr double kBacklogSpeedupFactor = 0.5;
};
```

- [ ] **Step 3: Write the basic ChatTextPacer implementation**

Create `apps/desktop/src/chat/ChatTextPacer.cpp`:

```cpp
#include "chat/ChatTextPacer.h"

#include <QChar>

ChatTextPacer::ChatTextPacer(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &ChatTextPacer::tick);
}

void ChatTextPacer::append(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    m_queue.append(text);
    if (!m_timer.isActive()) {
        m_timer.start(effectiveInterval());
    }
}

void ChatTextPacer::setMsPerChar(int value)
{
    if (value < 1) value = 1;
    if (value == m_msPerChar) return;
    m_msPerChar = value;
    if (m_timer.isActive()) {
        m_timer.start(effectiveInterval());
    }
}

int ChatTextPacer::effectiveInterval() const
{
    return m_msPerChar;
}

void ChatTextPacer::tick()
{
    if (m_queue.isEmpty()) {
        m_timer.stop();
        return;
    }

    // Surrogate pair: emit both halves together so emoji aren't sliced.
    int popCount = 1;
    const QChar first = m_queue.at(0);
    if (first.isHighSurrogate() && m_queue.size() >= 2) {
        const QChar second = m_queue.at(1);
        if (second.isLowSurrogate()) {
            popCount = 2;
        }
    }

    const QString chunk = m_queue.left(popCount);
    m_queue.remove(0, popCount);
    emit chunkReady(chunk);

    if (m_queue.isEmpty()) {
        m_timer.stop();
    }
}
```

(Backlog catch-up is added in Task 3 — for now `effectiveInterval()` just returns `m_msPerChar`.)

- [ ] **Step 4: Register the pacer in DESKTOP_SOURCES and add the smoke target**

Open `apps/desktop/CMakeLists.txt`. Find the `DESKTOP_SOURCES` block. After the existing `src/chat/ChatStreamEvent.h` line, append (alphabetical order):

```cmake
    src/chat/ChatTextPacer.cpp
    src/chat/ChatTextPacer.h
```

Then inside the `if(BUILD_TESTING)` block, after the existing `chat_stream_event_parser_smoke` test, append:

```cmake
    add_executable(ChatTextPacerSmoke
        tests/chat_text_pacer_smoke.cpp
        src/chat/ChatTextPacer.cpp
        src/chat/ChatTextPacer.h
    )

    target_include_directories(ChatTextPacerSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ChatTextPacerSmoke
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(NAME chat_text_pacer_smoke COMMAND ChatTextPacerSmoke)
```

The `Qt6::Test` link is needed for `QSignalSpy`. Verify the existing `find_package(Qt6 ...)` line at the top of the file includes the `Test` component; if not, add it. The existing line currently is:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Widgets Multimedia Network QuickControls2)
```

Replace with:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Widgets Multimedia Network QuickControls2 Test)
```

- [ ] **Step 5: Reconfigure and run the smoke test**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew && \
  cmake --build build --target ChatTextPacerSmoke && \
  ctest --test-dir build -R chat_text_pacer_smoke --output-on-failure
```

Expected: builds, test passes. If `Qt6::Test` isn't found, double-check the `find_package` line.

- [ ] **Step 6: Commit**

```bash
git add apps/desktop/src/chat/ChatTextPacer.h \
        apps/desktop/src/chat/ChatTextPacer.cpp \
        apps/desktop/tests/chat_text_pacer_smoke.cpp \
        apps/desktop/CMakeLists.txt
git commit -m "feat(desktop): ChatTextPacer basic streaming"
```

---

## Task 3: ChatTextPacer — Backlog Catch-up + msPerChar (TDD)

**Files:**
- Modify: `apps/desktop/tests/chat_text_pacer_smoke.cpp`
- Modify: `apps/desktop/src/chat/ChatTextPacer.cpp`

- [ ] **Step 1: Add the failing backlog assertion to the smoke test**

Open `apps/desktop/tests/chat_text_pacer_smoke.cpp`. Above `return 0;`, insert:

```cpp
    // --- Backlog catch-up: when queue exceeds maxBacklog, interval halves ---
    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(40);
        QSignalSpy chunks(&pacer, &ChatTextPacer::chunkReady);

        // 60 chars > maxBacklog (30) → effective interval should drop to 20 ms.
        // Total emit time should be roughly 60 * 20 = 1200 ms with backlog
        // speedup, vs 60 * 40 = 2400 ms without. We test the qualitative
        // property: the drain finishes faster than the no-speedup baseline.
        QString long_payload;
        for (int i = 0; i < 60; ++i) long_payload.append(QChar('x'));
        const qint64 start = QDateTime::currentMSecsSinceEpoch();
        pacer.append(long_payload);

        const bool drained = waitFor(app, 3000, [&]() { return pacer.pendingCount() == 0; });
        assert(drained);
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - start;

        // No-speedup baseline = 60 * 40 = 2400 ms. With backlog kicking in
        // around the 30-char mark, total time should be well under 2200 ms.
        assert(elapsed < 2200);
        assert(chunks.size() == 60);
    }

    // --- setMsPerChar updates an active timer interval ---
    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(200);
        pacer.append(QStringLiteral("yz"));
        pacer.setMsPerChar(15);
        const bool drained = waitFor(app, 1000, [&]() { return pacer.pendingCount() == 0; });
        assert(drained);
    }
```

At the top of the file, add `#include <QDateTime>` near the existing `#include <QTimer>`.

- [ ] **Step 2: Run to verify the new assertion fails**

Run:

```bash
cmake --build build --target ChatTextPacerSmoke && \
  ctest --test-dir build -R chat_text_pacer_smoke --output-on-failure
```

Expected: fails because the test now expects `elapsed < 2200` ms for a 60-char drain at 40 ms/char, which requires backlog speedup to be active.

- [ ] **Step 3: Implement backlog catch-up**

Open `apps/desktop/src/chat/ChatTextPacer.cpp`. Replace `effectiveInterval()` and `tick()` with:

```cpp
int ChatTextPacer::effectiveInterval() const
{
    if (m_queue.size() > kMaxBacklog) {
        const int sped = static_cast<int>(m_msPerChar * kBacklogSpeedupFactor);
        return sped < 1 ? 1 : sped;
    }
    return m_msPerChar;
}

void ChatTextPacer::tick()
{
    if (m_queue.isEmpty()) {
        m_timer.stop();
        return;
    }

    int popCount = 1;
    const QChar first = m_queue.at(0);
    if (first.isHighSurrogate() && m_queue.size() >= 2) {
        const QChar second = m_queue.at(1);
        if (second.isLowSurrogate()) {
            popCount = 2;
        }
    }

    const QString chunk = m_queue.left(popCount);
    m_queue.remove(0, popCount);
    emit chunkReady(chunk);

    if (m_queue.isEmpty()) {
        m_timer.stop();
        return;
    }

    // Adapt interval to current backlog level on each tick so the rate
    // returns to normal as the queue drains.
    const int next = effectiveInterval();
    if (m_timer.interval() != next) {
        m_timer.start(next);
    }
}
```

Also update `append()` to start with the effective interval (already does via `effectiveInterval()`; no change). Update `setMsPerChar` to use `effectiveInterval()` as well — already in place.

- [ ] **Step 4: Run the test**

Run:

```bash
cmake --build build --target ChatTextPacerSmoke && \
  ctest --test-dir build -R chat_text_pacer_smoke --output-on-failure
```

Expected: passes.

- [ ] **Step 5: Commit**

```bash
git add apps/desktop/tests/chat_text_pacer_smoke.cpp \
        apps/desktop/src/chat/ChatTextPacer.cpp
git commit -m "feat(desktop): ChatTextPacer backlog catch-up + dynamic msPerChar"
```

---

## Task 4: PetRuntime — Boundary / CleanFinish Notify API (TDD)

**Files:**
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`

- [ ] **Step 1: Add the failing smoke test block**

Open `apps/desktop/tests/pet_runtime_smoke.cpp`. Near the end of `main()` (before any `return 0` or final cleanup), insert:

```cpp
    // --- Phase 2.3.1: requestBoundaryAndNotify fires after handleAnimationFinished ---
    {
        PetRuntime runtime;
        runtime.setState(QStringLiteral("speaking"));
        // The runtime is now running a speaking action; the callback should NOT
        // fire synchronously.
        int boundaryHits = 0;
        runtime.requestBoundaryAndNotify([&boundaryHits]() { ++boundaryHits; });
        require(boundaryHits == 0,
                "requestBoundaryAndNotify must not fire while a speaking action is active");

        runtime.handleAnimationFinished();
        require(boundaryHits == 1,
                "requestBoundaryAndNotify callback should fire on the next animation boundary");
    }

    // --- requestBoundaryAndNotify on idle runtime fires synchronously ---
    {
        PetRuntime runtime;
        runtime.setState(QStringLiteral("idle"));
        int boundaryHits = 0;
        runtime.requestBoundaryAndNotify([&boundaryHits]() { ++boundaryHits; });
        require(boundaryHits == 1,
                "requestBoundaryAndNotify on idle runtime should fire synchronously");
    }

    // --- requestCleanFinishAndNotify behaves the same in Phase 2.3.1 ---
    {
        PetRuntime runtime;
        runtime.setState(QStringLiteral("speaking"));
        int hits = 0;
        runtime.requestCleanFinishAndNotify([&hits]() { ++hits; });
        require(hits == 0,
                "requestCleanFinishAndNotify must not fire while a speaking action is active");
        runtime.handleAnimationFinished();
        require(hits == 1,
                "requestCleanFinishAndNotify callback should fire on animation boundary");
    }
```

- [ ] **Step 2: Run to verify failure**

Run:

```bash
cmake --build build --target PetRuntimeSmoke 2>&1 | tail -10
```

Expected: build fails because `requestBoundaryAndNotify` and `requestCleanFinishAndNotify` don't exist yet.

- [ ] **Step 3: Add the API to PetRuntime.h**

Open `apps/desktop/src/pet/PetRuntime.h`. Find the existing `Q_INVOKABLE void requestExpression(...)` declarations near line 150. After the `handleAnimationFinished` declaration (around line 153), add:

```cpp
    // Phase 2.3.1: chat-side animation gating. Both fire `callback` on the
    // next animation boundary, or synchronously if the runtime is idle.
    //
    // Phase 2.3.1 they behave identically (boundary semantic). Phase 2.4 will
    // teach `requestCleanFinishAndNotify` to play `exit` phases before firing,
    // without changing the API — see docs/v2/设计方案/AI 聊天动画编排设计.md §6.
    void requestBoundaryAndNotify(std::function<void()> callback);
    void requestCleanFinishAndNotify(std::function<void()> callback);
```

At the top, add includes:

```cpp
#include <functional>
```

Inside the `private:` section, add (after the existing helper declarations):

```cpp
    void drainPendingNotifications();
    bool atAnimationBoundary() const;

    struct PendingNotification {
        std::function<void()> callback;
        QTimer *safetyTimer;
    };
    QList<PendingNotification> m_pendingNotifications;
```

Add `#include <QTimer>` if it's not already there (it is). Note that `QList<PendingNotification>` is needed; the existing file already pulls `<QList>`.

- [ ] **Step 4: Add the implementation in PetRuntime.cpp**

Open `apps/desktop/src/pet/PetRuntime.cpp`. Near the bottom of the file (after the existing `handleAnimationFinished` definition), append:

```cpp
namespace {
constexpr int kBoundarySafetyMs = 1500;
} // namespace

bool PetRuntime::atAnimationBoundary() const
{
    if (m_currentState == QStringLiteral("idle")) {
        return true;
    }
    if (m_currentActionId.isEmpty() && m_currentRecipeId.isEmpty()) {
        return true;
    }
    return false;
}

void PetRuntime::requestBoundaryAndNotify(std::function<void()> callback)
{
    if (!callback) {
        return;
    }
    if (atAnimationBoundary()) {
        callback();
        return;
    }

    QTimer *safety = new QTimer(this);
    safety->setSingleShot(true);
    PendingNotification entry{std::move(callback), safety};
    const int index = m_pendingNotifications.size();
    m_pendingNotifications.append(std::move(entry));

    connect(safety, &QTimer::timeout, this, [this, index]() {
        // Safety: if the index is still pending, fire it and remove.
        if (index < 0 || index >= m_pendingNotifications.size()) {
            return;
        }
        PendingNotification entry = m_pendingNotifications.takeAt(index);
        if (entry.safetyTimer) {
            entry.safetyTimer->deleteLater();
        }
        if (entry.callback) {
            entry.callback();
        }
    });
    safety->start(kBoundarySafetyMs);
}

void PetRuntime::requestCleanFinishAndNotify(std::function<void()> callback)
{
    // Phase 2.3.1: cleanFinish == boundary. Phase 2.4 will branch on
    // ActionDefinition::exitPhase to play the exit segment before notifying.
    requestBoundaryAndNotify(std::move(callback));
}

void PetRuntime::drainPendingNotifications()
{
    QList<PendingNotification> entries;
    entries.swap(m_pendingNotifications);
    for (PendingNotification &entry : entries) {
        if (entry.safetyTimer) {
            entry.safetyTimer->stop();
            entry.safetyTimer->deleteLater();
        }
        if (entry.callback) {
            entry.callback();
        }
    }
}
```

Then hook `drainPendingNotifications()` into the natural boundary point. Find the existing `void PetRuntime::handleAnimationFinished()` body (around line 425). At the end of its body — after the final `setState("idle")` or right before each `return` — call `drainPendingNotifications()`. The simplest placement is to make it the *first* statement so any pending callback fires whether or not a recipe step advances:

Replace the existing body:

```cpp
void PetRuntime::handleAnimationFinished()
{
    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    const PhaseDefinition phase = action.phases.value(m_currentPhaseId);
    if (!phase.nextPhase.isEmpty() && action.phases.contains(phase.nextPhase)) {
        playPhase(m_currentActionId, phase.nextPhase);
        return;
    }
    // ... existing logic
}
```

With:

```cpp
void PetRuntime::handleAnimationFinished()
{
    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    const PhaseDefinition phase = action.phases.value(m_currentPhaseId);
    if (!phase.nextPhase.isEmpty() && action.phases.contains(phase.nextPhase)) {
        playPhase(m_currentActionId, phase.nextPhase);
        return;
    }

    // 任何动画自然结束都算一次 boundary —— 即使后续会切到下一个 recipe step
    // 或回 idle，对 chat 状态机来说这一刻是可以放行新文字的。
    drainPendingNotifications();

    applyFacingAfterCurrentAction(action);

    if (!m_currentRecipeId.isEmpty()) {
        const RecipeDefinition recipe = m_manifest.recipes.value(m_currentRecipeId);
        if (m_currentRecipeStepIndex + 1 < recipe.steps.size()) {
            playNextRecipeStep();
            return;
        }

        clearActiveRecipe();
    }

    if (m_pendingRequest.kind != ActionRequestKind::None) {
        submitPendingActionRequest();
        return;
    }

    if (m_currentAutoReturnToIdle) {
        if (submitRuntimeEvent(PetEvent::actionCompleted(QRandomGenerator::global()->generateDouble()))) {
            return;
        }

        setState("idle");
    }
}
```

The exact existing body in the file is what you must preserve — only insert `drainPendingNotifications();` immediately after the phase-skip return, before the rest of the logic. If the body has drifted from what's shown above, locate the equivalent insertion point (right after the phase advance check, before `applyFacingAfterCurrentAction`).

- [ ] **Step 5: Run the test**

Run:

```bash
cmake --build build --target PetRuntimeSmoke && \
  ctest --test-dir build -R pet_runtime_smoke --output-on-failure
```

Expected: passes.

- [ ] **Step 6: Commit**

```bash
git add apps/desktop/src/pet/PetRuntime.h \
        apps/desktop/src/pet/PetRuntime.cpp \
        apps/desktop/tests/pet_runtime_smoke.cpp
git commit -m "feat(desktop): PetRuntime boundary/cleanFinish notify API"
```

---

## Task 5: ChatController — State Machine + BUFFERING_FOR_START → STREAMING

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Add failing assertions to the smoke test**

Open `apps/desktop/tests/chat_controller_smoke.cpp`. Near the existing hold-buffer block (around line 120), **before** the `return 0;` at the end, insert a new block:

```cpp
    // --- Phase 2.3.1: BUFFERING_FOR_START holds text until cleanFinishReady ---
    {
        PetRuntime smRuntime;
        smRuntime.setState(QStringLiteral("speaking")); // simulate non-idle baseline
        ChatController smController(&smRuntime, &settings);

        ChatStreamEvent smStarted;
        smStarted.type = QStringLiteral("RUN_STARTED");
        smController.applyStreamEvent(smStarted);
        // BUFFERING_FOR_START: text must be queued, not emitted to UI yet.

        ChatStreamEvent smExpr;
        smExpr.type = QStringLiteral("CUSTOM");
        smExpr.name = QStringLiteral("miles.pet.expression.requested");
        smExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        smExpr.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        smController.applyStreamEvent(smExpr);

        ChatStreamEvent smStart;
        smStart.type = QStringLiteral("TEXT_MESSAGE_START");
        smStart.role = QStringLiteral("assistant");
        smController.applyStreamEvent(smStart);

        ChatStreamEvent smText;
        smText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        smText.delta = QStringLiteral("异议!");
        smController.applyStreamEvent(smText);

        const auto messagesWhileBuffering = smController.messages();
        const QString textWhileBuffering = messagesWhileBuffering.constLast()
                .toMap().value(QStringLiteral("text")).toString();
        require(textWhileBuffering.isEmpty(),
                "BUFFERING_FOR_START must NOT push streamed text to the UI yet");

        // Simulate PetRuntime reaching a clean finish on the previous animation.
        smRuntime.handleAnimationFinished();
        // Pacer should now drain "异议!" into the assistant message at human pace.
        // We don't wait for the full drain here — just assert that the pacer
        // has something queued or has emitted at least one chunk.
        // (Full drain timing is covered by ChatTextPacerSmoke.)
    }
```

- [ ] **Step 2: Run the build — expect failure due to missing types**

Run:

```bash
cmake --build build --target ChatControllerSmoke 2>&1 | tail -10
```

Expected: compile failure on something the new ChatController types will provide (or the new block references the existing API but the test assertion fails because text already flushes immediately).

- [ ] **Step 3: Add state machine declarations to ChatController.h**

Open `apps/desktop/src/chat/ChatController.h`. After the existing `#include` block, add:

```cpp
#include <functional>
```

Forward-declare the pacer at the top, alongside the existing `class PetRuntime;` / `class SettingsService;`:

```cpp
class ChatTextPacer;
```

Inside the class definition, after the `Q_PROPERTY` block and before the constructor, add:

```cpp
public:
    enum class ChatPhase {
        IDLE,
        BUFFERING_FOR_START,
        STREAMING,
        GATED,
        WAITING_FOR_ANIMATION_END,
    };
```

Add the private members at the bottom of the existing `private:` section (replacing the existing `m_holdBuffer` comment block — we are repurposing the buffer for real gating now):

```cpp
    ChatPhase m_phase = ChatPhase::IDLE;
    QString m_pendingState;
    QString m_pendingExpression;
    ChatTextPacer *m_pacer = nullptr;
    QTimer m_gateTimeout;
    static constexpr int kGateTimeoutMs = 800;
```

Replace the comment above `QString m_holdBuffer;` to reflect the new role:

```cpp
    // Phase 2.3.1: while m_phase is BUFFERING_FOR_START or GATED, streamed
    // text accumulates here. On transition to STREAMING, it drains into the
    // pacer. See docs/v2/设计方案/AI 聊天动画编排设计.md §5.
    QString m_holdBuffer;
```

Add the new private method declarations (after the existing `flushHoldBuffer()`):

```cpp
    void transitionTo(ChatPhase next);
    void handleCleanFinishReady();
    void handleBoundaryReached();
    void handleGateTimeout();
    void drainHoldBufferToPacer();
    void appendChunkToCurrentMessage(const QString &chunk);
```

- [ ] **Step 4: Wire the pacer + gate timer in the ChatController constructor**

Open `apps/desktop/src/chat/ChatController.cpp`. Add an include at the top:

```cpp
#include "chat/ChatTextPacer.h"
```

In the constructor body (currently starts with `m_runtime(runtime)`), append after the existing `connect(...)` lines:

```cpp
    m_pacer = new ChatTextPacer(this);
    connect(m_pacer, &ChatTextPacer::chunkReady,
            this, &ChatController::appendChunkToCurrentMessage);
    if (m_settings != nullptr) {
        m_pacer->setMsPerChar(m_settings->msPerChar());
    }

    m_gateTimeout.setSingleShot(true);
    connect(&m_gateTimeout, &QTimer::timeout,
            this, &ChatController::handleGateTimeout);
```

In `handleSettingsSaved()`, after the existing `restartSidecar();` line, append:

```cpp
    if (m_settings != nullptr && m_pacer != nullptr) {
        m_pacer->setMsPerChar(m_settings->msPerChar());
    }
```

- [ ] **Step 5: Implement the state machine helpers (no event-handler changes yet)**

In `apps/desktop/src/chat/ChatController.cpp`, near the bottom of the file (after `failCurrentReply`), append:

```cpp
void ChatController::transitionTo(ChatPhase next)
{
    if (m_phase == next) {
        return;
    }
    m_phase = next;
    // Phase 2.3.1 keeps transitions internal — no signal needs to leak to QML.
}

void ChatController::handleCleanFinishReady()
{
    if (m_phase != ChatPhase::BUFFERING_FOR_START) {
        return;
    }
    if (!m_pendingExpression.isEmpty()) {
        requestPetExpression(m_pendingState, m_pendingExpression);
        m_pendingExpression.clear();
        m_pendingState.clear();
    }
    transitionTo(ChatPhase::STREAMING);
    drainHoldBufferToPacer();
}

void ChatController::handleBoundaryReached()
{
    m_gateTimeout.stop();
    if (m_phase == ChatPhase::GATED) {
        if (!m_pendingExpression.isEmpty()) {
            requestPetExpression(m_pendingState, m_pendingExpression);
            m_pendingExpression.clear();
            m_pendingState.clear();
        }
        transitionTo(ChatPhase::STREAMING);
        drainHoldBufferToPacer();
        return;
    }
    if (m_phase == ChatPhase::WAITING_FOR_ANIMATION_END) {
        if (m_runtime != nullptr) {
            m_runtime->returnToIdle();
        }
        transitionTo(ChatPhase::IDLE);
    }
}

void ChatController::handleGateTimeout()
{
    if (m_phase != ChatPhase::GATED) {
        return;
    }
    if (!m_pendingExpression.isEmpty()) {
        requestPetExpression(m_pendingState, m_pendingExpression);
        m_pendingExpression.clear();
        m_pendingState.clear();
    }
    transitionTo(ChatPhase::STREAMING);
    drainHoldBufferToPacer();
}

void ChatController::drainHoldBufferToPacer()
{
    if (m_holdBuffer.isEmpty() || m_pacer == nullptr) {
        return;
    }
    m_pacer->append(m_holdBuffer);
    m_holdBuffer.clear();
}

void ChatController::appendChunkToCurrentMessage(const QString &chunk)
{
    if (m_cancelled || chunk.isEmpty()) {
        return;
    }
    if (m_assistantMessageIndex < 0 || m_assistantMessageIndex >= m_messages.size()) {
        appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
        m_assistantMessageIndex = m_messages.size() - 1;
    }
    QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
    message.insert(QStringLiteral("text"),
                   message.value(QStringLiteral("text")).toString() + chunk);
    message.insert(QStringLiteral("pending"), true);
    m_messages[m_assistantMessageIndex] = message;
    emit messagesChanged();
}
```

- [ ] **Step 6: Wire RUN_STARTED → BUFFERING_FOR_START in applyStreamEvent**

Open `apps/desktop/src/chat/ChatController.cpp`. Find the existing handler for `RUN_STARTED`:

```cpp
    if (event.type == QStringLiteral("RUN_STARTED")) {
        setSending(true);
        setStatusText(QStringLiteral("正在回复"));
        return;
    }
```

Replace with:

```cpp
    if (event.type == QStringLiteral("RUN_STARTED")) {
        setSending(true);
        setStatusText(QStringLiteral("正在回复"));
        m_holdBuffer.clear();
        m_pendingExpression.clear();
        m_pendingState.clear();
        transitionTo(ChatPhase::BUFFERING_FOR_START);
        if (m_runtime != nullptr) {
            m_runtime->requestCleanFinishAndNotify([this]() {
                handleCleanFinishReady();
            });
        }
        return;
    }
```

Find the existing `TEXT_MESSAGE_CONTENT` handler:

```cpp
    if (event.type == QStringLiteral("TEXT_MESSAGE_CONTENT")) {
        appendAssistantDelta(event.delta);
        return;
    }
```

Replace with:

```cpp
    if (event.type == QStringLiteral("TEXT_MESSAGE_CONTENT")) {
        if (m_phase == ChatPhase::STREAMING) {
            if (m_pacer != nullptr) {
                m_pacer->append(event.delta);
            } else {
                m_holdBuffer.append(event.delta);
                flushHoldBuffer();
            }
        } else {
            m_holdBuffer.append(event.delta);
        }
        return;
    }
```

Find the existing CUSTOM `expression.requested` handler:

```cpp
    if (event.type == QStringLiteral("CUSTOM") && event.name == QString::fromLatin1(kExpressionRequestedEvent)) {
        requestPetExpression(event.value.value(QStringLiteral("state")).toString(),
                             event.value.value(QStringLiteral("expression")).toString(),
                             interruptHintFromValue(event.value));
    }
```

Replace with:

```cpp
    if (event.type == QStringLiteral("CUSTOM") && event.name == QString::fromLatin1(kExpressionRequestedEvent)) {
        const QString state = event.value.value(QStringLiteral("state")).toString();
        const QString expression = event.value.value(QStringLiteral("expression")).toString();

        if (m_phase == ChatPhase::BUFFERING_FOR_START
                || m_phase == ChatPhase::GATED) {
            // Store; will be applied on the next boundary callback.
            m_pendingState = state;
            m_pendingExpression = expression;
            return;
        }

        if (m_phase == ChatPhase::STREAMING) {
            m_pendingState = state;
            m_pendingExpression = expression;
            transitionTo(ChatPhase::GATED);
            m_gateTimeout.start(kGateTimeoutMs);
            if (m_runtime != nullptr) {
                m_runtime->requestBoundaryAndNotify([this]() {
                    handleBoundaryReached();
                });
            }
            return;
        }

        // IDLE / WAITING_FOR_ANIMATION_END: fall back to immediate apply
        // (matches pre-2.3.1 behavior and keeps the legacy path working).
        requestPetExpression(state, expression, interruptHintFromValue(event.value));
    }
```

- [ ] **Step 7: Build and run smoke tests**

Run:

```bash
cmake --build build --target ChatControllerSmoke && \
  ctest --test-dir build -R chat_controller_smoke --output-on-failure
```

Expected: the new "BUFFERING_FOR_START must NOT push streamed text to the UI yet" assertion passes. Earlier assertions about thinking/speaking expression state may still pass because the test calls `applyStreamEvent` directly without firing RUN_STARTED first in those blocks — verify all existing assertions still pass too.

If existing assertions fail, the most likely cause is that an existing test block sends `CUSTOM thinking` before `RUN_STARTED`; in that case the path falls through to the legacy `requestPetExpression` call by design. If the test fails for a different reason, debug before continuing.

- [ ] **Step 8: Commit**

```bash
git add apps/desktop/src/chat/ChatController.h \
        apps/desktop/src/chat/ChatController.cpp \
        apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "feat(desktop): ChatController state machine + BUFFERING_FOR_START gate"
```

---

## Task 6: ChatController — STREAMING ⇄ GATED (Mid-Run Expression)

**Files:**
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

(All ChatController code paths needed for this transition were added in Task 5; this task just verifies them with explicit assertions.)

- [ ] **Step 1: Add the mid-stream expression-switch assertion**

Open `apps/desktop/tests/chat_controller_smoke.cpp`. After the existing BUFFERING_FOR_START block added in Task 5, append:

```cpp
    // --- Phase 2.3.1: mid-stream expression switch (STREAMING → GATED → STREAMING) ---
    {
        PetRuntime gRuntime;
        ChatController gController(&gRuntime, &settings);

        // Drive the controller into STREAMING.
        ChatStreamEvent gStarted;
        gStarted.type = QStringLiteral("RUN_STARTED");
        gController.applyStreamEvent(gStarted);
        // Runtime starts at idle → cleanFinish fires synchronously → STREAMING.

        ChatStreamEvent gExpr1;
        gExpr1.type = QStringLiteral("CUSTOM");
        gExpr1.name = QStringLiteral("miles.pet.expression.requested");
        gExpr1.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        gExpr1.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        gController.applyStreamEvent(gExpr1);

        ChatStreamEvent gStart;
        gStart.type = QStringLiteral("TEXT_MESSAGE_START");
        gStart.role = QStringLiteral("assistant");
        gController.applyStreamEvent(gStart);

        ChatStreamEvent gText1;
        gText1.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        gText1.delta = QStringLiteral("片段1");
        gController.applyStreamEvent(gText1);
        // Currently STREAMING — text goes to pacer. The UI may not have seen it yet.

        // Mid-run expression event → enters GATED.
        ChatStreamEvent gExpr2;
        gExpr2.type = QStringLiteral("CUSTOM");
        gExpr2.name = QStringLiteral("miles.pet.expression.requested");
        gExpr2.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        gExpr2.value.insert(QStringLiteral("expression"), QStringLiteral("polite"));
        gController.applyStreamEvent(gExpr2);

        // While GATED, additional text must be buffered, not flushed.
        ChatStreamEvent gText2;
        gText2.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        gText2.delta = QStringLiteral("片段2");
        gController.applyStreamEvent(gText2);

        // Snapshot the current message text BEFORE the boundary fires.
        const QString preBoundaryText = gController.messages().constLast()
                .toMap().value(QStringLiteral("text")).toString();

        // Now drive the boundary callback.
        gRuntime.handleAnimationFinished();
        // After boundary: pacer should now have 片段2 enqueued and the next
        // expression should be requested. We don't wait for full pacer drain
        // (covered by ChatTextPacerSmoke), but we assert that the runtime now
        // reflects the second expression.
        require(gRuntime.currentState() == QStringLiteral("speaking"),
                "boundary callback should request the queued polite expression");
    }
```

- [ ] **Step 2: Build and run**

Run:

```bash
cmake --build build --target ChatControllerSmoke && \
  ctest --test-dir build -R chat_controller_smoke --output-on-failure
```

Expected: passes. The smoke test now exercises the mid-stream STREAMING → GATED → STREAMING transition.

- [ ] **Step 3: Commit**

```bash
git add apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "test(phase-2-3-1): cover STREAMING → GATED → STREAMING transition"
```

---

## Task 7: ChatController — STREAMING → WAITING_FOR_ANIMATION_END → IDLE on RUN_FINISHED

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Add the failing assertion**

Open `apps/desktop/tests/chat_controller_smoke.cpp`. After the GATED block from Task 6, append:

```cpp
    // --- Phase 2.3.1: RUN_FINISHED → WAITING_FOR_ANIMATION_END → IDLE ---
    {
        PetRuntime fRuntime;
        ChatController fController(&fRuntime, &settings);

        ChatStreamEvent fStarted;
        fStarted.type = QStringLiteral("RUN_STARTED");
        fController.applyStreamEvent(fStarted);

        ChatStreamEvent fExpr;
        fExpr.type = QStringLiteral("CUSTOM");
        fExpr.name = QStringLiteral("miles.pet.expression.requested");
        fExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        fExpr.value.insert(QStringLiteral("expression"), QStringLiteral("polite"));
        fController.applyStreamEvent(fExpr);

        ChatStreamEvent fStart;
        fStart.type = QStringLiteral("TEXT_MESSAGE_START");
        fStart.role = QStringLiteral("assistant");
        fController.applyStreamEvent(fStart);

        require(fController.sending(),
                "controller should be sending after RUN_STARTED");

        ChatStreamEvent fFinished;
        fFinished.type = QStringLiteral("RUN_FINISHED");
        fController.applyStreamEvent(fFinished);
        require(!fController.sending(),
                "RUN_FINISHED should clear the sending flag");
        require(fRuntime.currentState() == QStringLiteral("speaking"),
                "RUN_FINISHED must not interrupt the active speaking animation");

        // After the next boundary, the controller should return to idle.
        fRuntime.handleAnimationFinished();
        require(fRuntime.currentState() == QStringLiteral("idle"),
                "boundary after RUN_FINISHED should return PetRuntime to idle");
    }
```

- [ ] **Step 2: Build the test — expect failure**

Run:

```bash
cmake --build build --target ChatControllerSmoke 2>&1 | tail -10
```

Expected: the new block fails because `RUN_FINISHED` doesn't yet route through the state machine — the existing handler calls `finishCurrentReply()` which clears sending but doesn't request boundary or transition to WAITING_FOR_ANIMATION_END.

- [ ] **Step 3: Route RUN_FINISHED through the state machine**

Open `apps/desktop/src/chat/ChatController.cpp`. Find the existing `RUN_FINISHED` handler:

```cpp
    if (event.type == QStringLiteral("RUN_FINISHED")) {
        finishCurrentReply();
        return;
    }
```

Replace with:

```cpp
    if (event.type == QStringLiteral("RUN_FINISHED")) {
        // Clear sending + status now; pet animation still needs to settle.
        if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
            QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
            message.insert(QStringLiteral("pending"), false);
            m_messages[m_assistantMessageIndex] = message;
            emit messagesChanged();
        }
        m_currentReply.clear();
        setSending(false);
        setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));

        // Drain any text held back by an unresolved GATED state so the user
        // sees everything the model sent before we ask the pet to settle.
        drainHoldBufferToPacer();

        transitionTo(ChatPhase::WAITING_FOR_ANIMATION_END);
        if (m_runtime != nullptr) {
            m_runtime->requestBoundaryAndNotify([this]() {
                handleBoundaryReached();
            });
        } else {
            transitionTo(ChatPhase::IDLE);
        }
        return;
    }
```

- [ ] **Step 4: Build and run**

Run:

```bash
cmake --build build --target ChatControllerSmoke && \
  ctest --test-dir build -R chat_controller_smoke --output-on-failure
```

Expected: the new assertion passes. Earlier assertions about `RUN_FINISHED should not interrupt the current speaking state` should still pass because the state machine no longer triggers `returnToIdle` synchronously — it waits for the boundary.

Note: the existing assertion `RUN_FINISHED should not restart or replace the current speaking action` should still hold because no new action is requested by the state machine; only `requestBoundaryAndNotify` is called, which doesn't start playback.

- [ ] **Step 5: Commit**

```bash
git add apps/desktop/src/chat/ChatController.cpp \
        apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "feat(desktop): ChatController WAITING_FOR_ANIMATION_END on RUN_FINISHED"
```

---

## Task 8: ChatController — Cancel / Error / Gate Timeout Paths

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Add the failing assertions**

Open `apps/desktop/tests/chat_controller_smoke.cpp`. After the Task 7 block, append:

```cpp
    // --- Phase 2.3.1: cancel during GATED drains hold buffer and returns to IDLE ---
    {
        PetRuntime cRuntime;
        ChatController cController(&cRuntime, &settings);

        ChatStreamEvent cStarted;
        cStarted.type = QStringLiteral("RUN_STARTED");
        cController.applyStreamEvent(cStarted);

        ChatStreamEvent cExpr;
        cExpr.type = QStringLiteral("CUSTOM");
        cExpr.name = QStringLiteral("miles.pet.expression.requested");
        cExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        cExpr.value.insert(QStringLiteral("expression"), QStringLiteral("polite"));
        cController.applyStreamEvent(cExpr);

        ChatStreamEvent cStart;
        cStart.type = QStringLiteral("TEXT_MESSAGE_START");
        cStart.role = QStringLiteral("assistant");
        cController.applyStreamEvent(cStart);

        ChatStreamEvent cText;
        cText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        cText.delta = QStringLiteral("片段");
        cController.applyStreamEvent(cText);

        // Now drive into GATED via a second expression event.
        ChatStreamEvent cExpr2;
        cExpr2.type = QStringLiteral("CUSTOM");
        cExpr2.name = QStringLiteral("miles.pet.expression.requested");
        cExpr2.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        cExpr2.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        cController.applyStreamEvent(cExpr2);

        ChatStreamEvent cText2;
        cText2.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        cText2.delta = QStringLiteral("还有");
        cController.applyStreamEvent(cText2);

        // User cancels mid-stream.
        cController.cancelCurrentReply();
        require(!cController.sending(),
                "cancel should clear sending");
        require(cRuntime.currentState() == QStringLiteral("idle"),
                "cancel should ask the pet to return to idle");
    }
```

- [ ] **Step 2: Build the test — expect failure**

Run:

```bash
cmake --build build --target ChatControllerSmoke 2>&1 | tail -10
```

Expected: assertion `cancel should ask the pet to return to idle` fails because the existing `cancelCurrentReply` calls `requestPetExpression("idle", "neutral")` synchronously without going through `returnToIdle`, but PetRuntime is currently in `speaking` state with an active action.

(If this assertion already passes due to the existing `requestPetExpression(idle, neutral)` call, the test still adds value as a regression guard. Move on to Step 3.)

- [ ] **Step 3: Update cancelCurrentReply to drain + clean-finish**

Open `apps/desktop/src/chat/ChatController.cpp`. Find the existing `cancelCurrentReply()` body:

```cpp
void ChatController::cancelCurrentReply()
{
    if (!m_sending && m_currentReply.isNull()) {
        return;
    }

    m_cancelled = true;
    m_holdBuffer.clear();
    if (m_currentReply) {
        QNetworkReply *reply = m_currentReply;
        m_currentReply.clear();
        reply->abort();
        reply->deleteLater();
    }

    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
        message.insert(QStringLiteral("pending"), false);
        m_messages[m_assistantMessageIndex] = message;
        emit messagesChanged();
    }

    m_assistantMessageIndex = -1;
    setSending(false);
    setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    requestPetExpression(QStringLiteral("idle"), QStringLiteral("neutral"));
}
```

Replace with:

```cpp
void ChatController::cancelCurrentReply()
{
    if (!m_sending && m_currentReply.isNull() && m_phase == ChatPhase::IDLE) {
        return;
    }

    m_cancelled = true;
    // Per design §5.4: drain any buffered text into the pacer so the user
    // doesn't lose visible content already on the way. The pacer will keep
    // emitting at human pace (with backlog catch-up if necessary).
    drainHoldBufferToPacer();
    m_gateTimeout.stop();

    if (m_currentReply) {
        QNetworkReply *reply = m_currentReply;
        m_currentReply.clear();
        reply->abort();
        reply->deleteLater();
    }

    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
        message.insert(QStringLiteral("pending"), false);
        m_messages[m_assistantMessageIndex] = message;
        emit messagesChanged();
    }

    m_assistantMessageIndex = -1;
    setSending(false);
    setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    transitionTo(ChatPhase::IDLE);
    if (m_runtime != nullptr) {
        m_runtime->returnToIdle();
    }
}
```

Find the existing `failCurrentReply()` body. After `setStatusText(QStringLiteral("错误"));`, replace the trailing `requestPetExpression(QStringLiteral("error"), QStringLiteral("neutral"));` with:

```cpp
    requestPetExpression(QStringLiteral("error"), QStringLiteral("neutral"));
    drainHoldBufferToPacer();
    m_gateTimeout.stop();
    transitionTo(ChatPhase::IDLE);
```

- [ ] **Step 4: Build and run**

Run:

```bash
cmake --build build --target ChatControllerSmoke && \
  ctest --test-dir build -R chat_controller_smoke --output-on-failure
```

Expected: all assertions pass.

- [ ] **Step 5: Commit**

```bash
git add apps/desktop/src/chat/ChatController.cpp \
        apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "feat(desktop): ChatController cancel/error drain + clean-finish"
```

---

## Task 9: Integrate ChatTextPacer — Replace Immediate Flush

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

(The pacer is already constructed and connected in Task 5. This task makes sure the existing `appendAssistantDelta` / `flushHoldBuffer` legacy path is fully retired so all text now flows through the pacer.)

- [ ] **Step 1: Confirm the existing tests still rely on synchronous text**

Open `apps/desktop/tests/chat_controller_smoke.cpp`. The original hold-buffer block (around line 120) asserts:

```cpp
require(hbController.messages().constFirst().toMap().value("text").toString()
            == QStringLiteral("片段一"),
        "hold buffer should flush content immediately in phase 2.1");
```

This was Phase 2.1 plumbing that no longer holds — Phase 2.3.1 routes streamed text through the pacer at `msPerChar` rate, so the text won't appear synchronously.

Update those two assertions (the "片段一" and "片段一片段二" blocks) to wait for the pacer to drain. Add a small helper at the top of the file (inside the anonymous namespace):

```cpp
// Spin a Qt event loop until predicate() is true or timeoutMs elapses.
bool waitFor(int timeoutMs, auto predicate)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return predicate();
}
```

Add `#include <QElapsedTimer>` at the top.

Then replace the original hold-buffer block's two assertions with:

```cpp
    // Drive the controller into STREAMING by starting and acknowledging cleanFinish.
    ChatStreamEvent hbStarted;
    hbStarted.type = QStringLiteral("RUN_STARTED");
    hbController.applyStreamEvent(hbStarted);

    ChatStreamEvent hbStart;
    hbStart.type = QStringLiteral("TEXT_MESSAGE_START");
    hbStart.role = QStringLiteral("assistant");
    hbController.applyStreamEvent(hbStart);

    ChatStreamEvent hbContent;
    hbContent.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
    hbContent.delta = QStringLiteral("片段一");
    hbController.applyStreamEvent(hbContent);

    require(waitFor(2000, [&]() {
                return hbController.messages().constFirst().toMap()
                        .value(QStringLiteral("text")).toString()
                        == QStringLiteral("片段一");
            }),
            "pacer should eventually deliver '片段一' to the UI");

    hbContent.delta = QStringLiteral("片段二");
    hbController.applyStreamEvent(hbContent);
    require(waitFor(2000, [&]() {
                return hbController.messages().constFirst().toMap()
                        .value(QStringLiteral("text")).toString()
                        == QStringLiteral("片段一片段二");
            }),
            "subsequent deltas should accumulate through the pacer");
```

The block needed `hbRuntime` to be at idle so `RUN_STARTED` flips straight to STREAMING via the synchronous cleanFinish path. The existing block constructs a fresh `PetRuntime`, which by default starts in the idle state (verified in earlier assertions), so this works.

Make sure `settings.setMsPerChar(40)` is called before this block — the smoke test's default is 80 ms, but for the test we want faster drain. Add right after the `settings.setBaseUrl(...)` line at the top of `main`:

```cpp
    settings.setMsPerChar(20);
```

- [ ] **Step 2: Build and run the controller smoke test**

Run:

```bash
cmake --build build --target ChatControllerSmoke && \
  ctest --test-dir build -R chat_controller_smoke --output-on-failure
```

Expected: passes. The pacer now drives all text.

- [ ] **Step 3: Update the controller smoke test target to link ChatTextPacer**

Open `apps/desktop/CMakeLists.txt`. Find the `qt_add_executable(ChatControllerSmoke ...)` block. Add the pacer sources:

```cmake
        src/chat/ChatTextPacer.cpp
        src/chat/ChatTextPacer.h
```

(Add them alphabetically inside the existing source list of that target.)

- [ ] **Step 4: Reconfigure and rerun all C++ tests**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew && \
  ctest --test-dir build -R "chat_controller_smoke|chat_text_pacer_smoke|pet_runtime_smoke|settings_service_smoke" --output-on-failure
```

Expected: all four pass.

- [ ] **Step 5: Remove the now-unused legacy text flush path**

`appendAssistantDelta` and the immediate `flushHoldBuffer` call from the TEXT_MESSAGE_CONTENT handler are no longer reachable in the normal flow (text goes through pacer when STREAMING, sits in hold buffer when buffered/gated, never via the immediate flush). However, `flushHoldBuffer` is still useful internally — it appends `m_holdBuffer` to the current assistant message directly. Replace the body of `appendAssistantDelta` and `flushHoldBuffer` to keep them no-ops or remove them entirely.

Cleanest approach: delete `appendAssistantDelta` and `flushHoldBuffer` (both the declaration in `.h` and the definition in `.cpp`), since nothing calls them anymore. Quick check before deletion:

```bash
grep -n "appendAssistantDelta\|flushHoldBuffer" apps/desktop/src apps/desktop/tests
```

Expected: matches only the declarations and definitions themselves. If anything else references them, leave them in place and just comment them as deprecated.

Assuming no external references, delete:
- The declaration `void appendAssistantDelta(const QString &delta);` from `ChatController.h`.
- The declaration `void flushHoldBuffer();` from `ChatController.h`.
- The definitions of both methods from `ChatController.cpp`.

- [ ] **Step 6: Rebuild to confirm nothing else broke**

Run:

```bash
cmake --build build --target ChatControllerSmoke && \
  cmake --build build --target MilesEdgeworthDesktop && \
  ctest --test-dir build -R "chat_controller_smoke|chat_text_pacer_smoke" --output-on-failure
```

Expected: builds clean, tests pass.

- [ ] **Step 7: Commit**

```bash
git add apps/desktop/src/chat/ChatController.h \
        apps/desktop/src/chat/ChatController.cpp \
        apps/desktop/tests/chat_controller_smoke.cpp \
        apps/desktop/CMakeLists.txt
git commit -m "feat(desktop): route all streamed text through ChatTextPacer"
```

---

## Task 10: Final Verification

**Files:**
- Read-only.

- [ ] **Step 1: Run the contract check**

Run:

```bash
python3 tests/check_phase_2_3_1_animation_sync.py
```

Expected: `phase 2.3.1 animation-text sync contract ok`. If any assertion fails, fix the corresponding file inline (do not relax assertions).

- [ ] **Step 2: Run all CTest checks**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop && \
  ctest --test-dir build --output-on-failure \
    -R "chat_text_pacer_smoke|chat_controller_smoke|pet_runtime_smoke|settings_service_smoke|chat_stream_event_parser_smoke|skin_manifest_loader_smoke|check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider|check_phase_2_2_settings|check_phase_2_3_1_animation_sync"
```

Expected: every listed test passes.

- [ ] **Step 3: Run the Go sidecar tests for regression**

Run:

```bash
(cd apps/agent-core && go test ./...)
```

Expected: all green. Phase 2.3.1 changes nothing on the sidecar side; this is a safety net.

- [ ] **Step 4: Manual end-to-end test**

If a real OpenAI-compatible endpoint is configured:

1. Launch the desktop app.
2. Send a message like "解释什么是检察官的责任，分两段回答" — a reply that should switch expressions mid-stream.
3. Observe:
   - Text emerges character-by-character at the configured `msPerChar` (default 80 ms).
   - When the model switches expression (sidecar emits a new `miles.pet.expression.requested`), the current animation loop completes once before the new expression's animation starts; text pauses during the transition.
   - When the model finishes, the current animation completes one more loop before the pet returns to idle.
4. Cancel a long reply mid-stream:
   - The buffered text continues to drain into the UI (don't lose visible content).
   - The pet animates back to idle naturally.
5. Open Settings → 文字节奏 → drag to 40 ms/字 → save. Send another message. Verify text now emerges noticeably faster.

Document any observed issues here before declaring done.

- [ ] **Step 5: No commit (verification only)**

---

## Task 11: Stage Record + Docs

**Files:**
- Create: `docs/v2/阶段记录/Phase 2.3.1 动画-文字同步.md`
- Modify: `docs/v2/文档索引.md`
- Modify: `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md`

- [ ] **Step 1: Write the stage record**

Create `docs/v2/阶段记录/Phase 2.3.1 动画-文字同步.md`:

```markdown
# Phase 2.3.1 动画-文字同步

本文记录 Phase 2.3.1 的实现范围、验收方式和已知限制。Phase 2.3.1 把 ChatController 的"立即 flush"模型升级为 5 状态门控状态机，并落地字符速率限制器和 PetRuntime 的边界通知 API。Phase 2.3 剩余两块（完整 Miles persona + SQLite 会话历史、Langfuse 接入）由 2.3.2 / 2.3.3 单独完成。

参考设计文档：`docs/v2/设计方案/AI 聊天动画编排设计.md` §5–7。

## 完成范围

- 新增 `apps/desktop/src/chat/ChatTextPacer.{h,cpp}`：QObject + QTimer 驱动的字符速率限制器，从 `SettingsService::msPerChar()` 取值，队列超过 30 字时把间隔减半。surrogate pair 一次性 emit，避免 emoji 拆字。
- ChatController 引入 `enum class ChatPhase { IDLE, BUFFERING_FOR_START, STREAMING, GATED, WAITING_FOR_ANIMATION_END }`。各状态的事件处理与设计文档 §5.3 一致：
  - `RUN_STARTED` → `BUFFERING_FOR_START`，调 `requestCleanFinishAndNotify`。
  - `cleanFinishReady` → 应用 pending expression，转 `STREAMING`，hold buffer → pacer。
  - 期间 expression 事件 → `GATED`，800ms safety timeout。
  - `boundaryReached` → 应用 pending expression，转 `STREAMING`，hold buffer → pacer。
  - `RUN_FINISHED` → `WAITING_FOR_ANIMATION_END`，调 `requestBoundaryAndNotify`。
  - 该状态下 `boundaryReached` → 调 `returnToIdle`，转 `IDLE`。
  - 用户取消 / provider 出错 / 网络断开 → drain hold buffer → pacer，转 `IDLE`，调 `returnToIdle`。
- PetRuntime 新增 `requestBoundaryAndNotify(std::function<void()>)` 与 `requestCleanFinishAndNotify(std::function<void()>)`，二者在 2.3.1 行为一致（boundary 语义）。在 `handleAnimationFinished` 自然边界处 drain 所有 pending callback，1500ms 安全超时兜底。
- ChatController 听 `SettingsController::saved` 后同步把新 `msPerChar` 推给 pacer。
- 新增 `chat_text_pacer_smoke` ctest 覆盖基础流、CJK、backlog 加速、msPerChar 动态调整。
- 扩展 `chat_controller_smoke` 覆盖 BUFFERING_FOR_START / STREAMING / GATED / WAITING_FOR_ANIMATION_END 全部转移以及 cancel 兜底。
- 扩展 `pet_runtime_smoke` 覆盖 boundary callback 同步 / 异步两条路径。
- 新增 `tests/check_phase_2_3_1_animation_sync.py` 静态契约。

## 验收命令

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
python3 tests/check_phase_2_1_provider.py
python3 tests/check_phase_2_2_settings.py
python3 tests/check_phase_2_3_1_animation_sync.py
(cd apps/agent-core && go test ./...)
cmake --build build --target MilesEdgeworthDesktop
ctest --test-dir build -R "chat_text_pacer_smoke|chat_controller_smoke|pet_runtime_smoke|settings_service_smoke|check_phase_2_3_1_animation_sync" --output-on-failure
```

手动验收：发一段需要切换 expression 的消息 → 观察动画到自然边界后再切表达，文字按 msPerChar 节奏出 → RUN_FINISHED 后动画再循环一轮自然回 idle → 取消时已到达的文字继续吐完，但桌宠平滑回 idle。

## 关键决策

- **状态机直接落在 ChatController 内**：拆出 `ChatStateMachine` 类是过度抽象——状态机只为 ChatController 一个使用者服务，事件入口也只有 `applyStreamEvent` 一条。状态机数据 + handler 留在 ChatController 内可读性反而更好。
- **`requestCleanFinishAndNotify` 与 `requestBoundaryAndNotify` 在 2.3.1 行为一致**：Phase 2.3.1 没有 phased 动画，两者都是"下一次自然边界回调"。Phase 2.4 phased 动画落地后，`requestCleanFinishAndNotify` 内部会先播 `exit` phase 再回调，不改 API。
- **取消时 drain hold buffer**：按设计文档 §5.4 "不丢用户已经看到一半的回复"。如果用户取消时 pacer 队列很大，backlog 加速会自动缩短可见拖尾时间。
- **`drainPendingNotifications` 在每次 `handleAnimationFinished` 都触发**：当前 Miles 皮肤的 speaking 动作都是单段 `loop` 或 `oneshot`，handleAnimationFinished 即代表"用户体感的边界"。Phase 2.4 phased 动画落地后会引入"区分 phase 内部 transition vs 真正边界"的逻辑——届时 PetRuntime 一侧扩展即可，chat 一侧不动。

## 当前限制

- 文字节奏只有"线性 msPerChar + backlog 半速"两段，没有按标点动态停顿、没有按句末减速。如果模型 dump 整段，目前靠 backlog 加速兜底，体验仍可接受。后续如果觉得不够拟人，可在 ChatTextPacer 加 punctuation pause。
- Pacer emit 单位是 QChar：BMP 内的 CJK / Latin 都是 1 char；surrogate pair（emoji 等）按对 emit。如果模型大量输出超出 BMP 的字符，体验等价于"一次 emit 2 char"。
- `requestCleanFinishAndNotify` 暂时不播 exit phase（Miles 皮肤当前 speaking 动作没有 exit 段）。Phase 2.4 phased 动画落地后这条路径才会有可见差异。
- 没有完整的 Miles persona（仍是 Phase 2.1 留下的最小版本）；没有会话历史；没有 Langfuse。这三块由 Phase 2.3.2 / 2.3.3 接管。

## 后续入口

- Phase 2.3.2：完整 Miles persona prompt + SQLite 会话历史 + 新建 / 清空会话 + 历史截断策略。
- Phase 2.3.3：Go sidecar provider 层接入 Langfuse SDK。
- Phase 2.4：phased 动画（enter / loop / exit），PetRuntime `requestCleanFinishAndNotify` 升级为播 exit 段。
```

- [ ] **Step 2: Update the doc index**

Open `docs/v2/文档索引.md`. Find the existing Phase 2.2 entry under 阶段记录:

```markdown
- [Phase 2.2 用户配置与安全存储](阶段记录/Phase%202.2%20用户配置与安全存储.md)
```

(or the form actually present). Append immediately after it:

```markdown
- [Phase 2.3.1 动画-文字同步](阶段记录/Phase%202.3.1%20动画-文字同步.md):Phase 2.3.1 的 ChatController 状态机、字符速率限制器和 PetRuntime 边界通知 API 验收记录。
```

- [ ] **Step 3: Add a "进行中" callout under Phase 2.3 in the rough plan**

Open `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md`. Find the Phase 2.3 section header:

```markdown
### Phase 2.3：会话历史与人设
```

Immediately below it, insert:

```markdown
> 进行中。动画-文字同步部分（状态机 + 字符速率限制器 + PetRuntime 边界通知）已在 `阶段记录/Phase 2.3.1 动画-文字同步.md` 完成。剩余子阶段：
> - **Phase 2.3.2**：完整 Miles persona + SQLite 会话历史 + 新建 / 清空对话 + 历史截断。
> - **Phase 2.3.3**：Langfuse 可观测性接入（Phase 2.1 预留的 middleware hook 激活）。
```

- [ ] **Step 4: Run the contract check + commit**

Run:

```bash
python3 tests/check_phase_2_3_1_animation_sync.py
```

Expected: `phase 2.3.1 animation-text sync contract ok`.

Commit:

```bash
git add docs/v2/阶段记录/Phase\ 2.3.1\ 动画-文字同步.md \
        docs/v2/文档索引.md \
        docs/v2/阶段记录/Phase\ 2\ AI\ 聊天粗规划.md
git commit -m "docs(phase-2-3-1): stage record + index + rough plan callout"
```

---

## Self-Review

### 1. Spec coverage

Walking through Phase 2.3.1 scope and the design doc:

- **ChatController 5-state machine** → declared in Task 5 Step 3 (`ChatPhase` enum), transitions wired in Task 5 Step 5–6 (BUFFERING_FOR_START, STREAMING), Task 6 (GATED), Task 7 (WAITING_FOR_ANIMATION_END), Task 8 (cancel → IDLE).
- **字符速率限制器** (设计文档 §7) → ChatTextPacer in Task 2 (basic), Task 3 (backlog catch-up + msPerChar setter).
- **PetRuntime requestBoundaryAndNotify / requestCleanFinishAndNotify** (设计文档 §6) → Task 4 with safety timeout.
- **800 ms GATED safety timeout** (设计文档 §6.3) → ChatController.h `kGateTimeoutMs` in Task 5 Step 3, `handleGateTimeout()` in Task 5 Step 5.
- **取消 / 错误时 drain hold buffer 到 pacer** (设计文档 §5.4) → Task 8 Step 3.
- **msPerChar 来自 SettingsService** → ChatController constructor wiring in Task 5 Step 4.
- **`SettingsController::saved` → pacer.setMsPerChar 同步** → Task 5 Step 4's `handleSettingsSaved` update.

### 2. Placeholder scan

Scanning for forbidden patterns: no "TBD" / "implement later" / "Add appropriate error handling" / "Similar to Task N" appear. Every code step has either a complete file body or an exact find/replace pair.

### 3. Type consistency

- `ChatPhase` enum values `IDLE, BUFFERING_FOR_START, STREAMING, GATED, WAITING_FOR_ANIMATION_END` — used identically in Task 5 (header), Task 5/6/7/8 (transitions), Task 1 (contract check assertions). ✓
- `ChatTextPacer` API: `append(const QString &)`, `setMsPerChar(int)`, `msPerChar() const`, `pendingCount() const`, signal `chunkReady(const QString &)` — used identically in Task 2 (header), Task 2/3 (impl), Task 5 (controller integration), Task 9 (smoke test). ✓
- `PetRuntime::requestBoundaryAndNotify(std::function<void()>)` and `requestCleanFinishAndNotify(std::function<void()>)` — declared in Task 4 Step 3, defined in Task 4 Step 4, called in Task 5 Step 6, Task 7 Step 3, Task 8 Step 3. ✓
- `PetRuntime::drainPendingNotifications()` — defined in Task 4 Step 4, hooked into `handleAnimationFinished`. ✓
- ChatController state machine private methods `transitionTo`, `handleCleanFinishReady`, `handleBoundaryReached`, `handleGateTimeout`, `drainHoldBufferToPacer`, `appendChunkToCurrentMessage` — declared in Task 5 Step 3, defined in Task 5 Step 5. ✓

No inconsistencies found.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-22-phase-2-3-1-animation-text-sync.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
