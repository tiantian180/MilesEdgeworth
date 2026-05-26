# Phase 2.5 Movement API Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Phase 2.5 target-point pet movement and OpenAI-compatible `pet_motion` tool use according to `docs/v2/设计方案/移动系统与工具调用设计.md`.

**Architecture:** Add a focused Qt `MotionController` that computes target movement but never touches windows directly. `PetRuntime` owns MotionController and translates motion state into locomotion animations; `DesktopShellController` and `main.cpp` bridge actual window geometry. The Go agent core owns tool registration, provider tool-call aggregation, `/v1/chat/tool-result`, and the multi-call tool loop; Qt `ChatController` serializes text/animation completion before executing movement.

**Tech Stack:** Qt 6.5+ / C++17 / QTimer / QElapsedTimer / Qt Network / CMake / CTest / Go 1.x / net/http / OpenAI-compatible Chat Completions SSE.

---

## Scope Check

Included:

- `pet_motion` tool with `moveTo` and `moveBy`.
- Qt target movement in reachable screen-percentage coordinates.
- Locomotion animation loop override during target movement.
- Drag, cancel, and timeout interruption paths.
- Sidecar tool-call aggregation and tool-result continuation loop.
- ChatController `EXECUTING_TOOL` state and tool result POST.
- Static contract checks for the Phase 2.5 boundaries.

Excluded:

- No `stop` action exposed to the model.
- No random wander rewrite; existing `BehaviorTriggerEngine` random locomotion remains frame-driven.
- No path planning, obstacle avoidance, multi-screen routing, MCP, plugin, or agent harness.
- No visual redesign of chat UI beyond the disabled input placeholder while executing a tool.

## Required Reading

Read these before editing:

- `docs/v2/设计方案/移动系统与工具调用设计.md`
- `docs/v2/设计方案/AI 聊天动画编排设计.md` §6-§7
- `docs/v2/设计方案/桌宠运行时与动画调度设计.md`
- `docs/v2/设计方案/桌宠运行时职责拆分设计.md`
- `docs/v2/设计方案/皮肤包播放行为设计.md`
- `apps/desktop/src/pet/PetRuntime.{h,cpp}`
- `apps/desktop/src/pet/PetRuntimeCleanFinish.cpp`
- `apps/desktop/src/chat/ChatController.{h,cpp}`
- `apps/desktop/src/chat/ChatStreamEvent.{h,cpp}`
- `apps/desktop/src/DesktopShellController.{h,cpp}`
- `apps/desktop/src/pet/events/PetEventBridge.{h,cpp}`
- `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`
- `apps/agent-core/internal/chat/provider.go`
- `apps/agent-core/internal/chat/openai/provider.go`
- `apps/agent-core/internal/chat/service/service.go`
- `apps/agent-core/internal/api/server.go`

## File Structure

Create:

- `apps/desktop/src/pet/motion/MotionController.h`
  - Testable target movement API and signal contract.
- `apps/desktop/src/pet/motion/MotionController.cpp`
  - Coordinate conversion, target clamp, direction quantization, timer tick loop.
- `apps/desktop/tests/motion_controller_smoke.cpp`
  - C++ smoke test for moveTo, moveBy, clamp, interruption, and direction selection.
- `apps/agent-core/internal/chat/service/tools.go`
  - Tool definitions, `pet_motion` schema, tool result registry, and timeout constants.
- `tests/check_phase_2_5_movement_tool.py`
  - Static contract check across Qt, Go, manifest, and CMake.

Modify:

- `apps/desktop/CMakeLists.txt`
  - Compile MotionController into app and tests; register `MotionControllerSmoke`.
- `CMakeLists.txt`
  - Register `check_phase_2_5_movement_tool`.
- `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
  - Add `motion` defaults.
- `apps/desktop/src/pet/manifest/SkinManifest.h`
  - Add `MotionDefinition`.
- `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
  - Parse `motion`.
- `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
  - Assert motion defaults parse.
- `apps/desktop/src/pet/PetRuntime.{h,cpp}`
  - Own MotionController; add `requestMotion`; add moving state and locomotion loop override.
- `apps/desktop/src/pet/PetRuntimeSkin.cpp`
  - Reconfigure motion when skin reloads and size changes.
- `apps/desktop/tests/pet_runtime_smoke.cpp`
  - Assert motion request enters moving state, loops locomotion, completes/interrupted paths restore idle semantics.
- `apps/desktop/src/DesktopShellController.{h,cpp}`
  - Expose motion screen geometry for main.cpp wiring.
- `apps/desktop/src/main.cpp`
  - Connect runtime motion position output to shell movement, and shell geometry to runtime motion environment.
- `apps/desktop/src/pet/events/PetEventBridge.cpp`
  - Call motion cancel before drag gesture tracking.
- `apps/desktop/src/chat/ChatStreamEvent.{h,cpp}`
  - Parse `TOOL_CALL` fields.
- `apps/desktop/tests/chat_stream_event_parser_smoke.cpp`
  - Assert compact `TOOL_CALL` parsing.
- `apps/desktop/src/chat/ChatController.{h,cpp}`
  - Add `EXECUTING_TOOL`, parse/validate `pet_motion`, call runtime, POST tool result, handle timeout/cancel.
- `apps/desktop/tests/chat_controller_smoke.cpp`
  - Assert tool-only path does not hit start timeout and that unknown tools produce result without motion.
- `apps/desktop/qml/ChatComposer.qml`
  - Show executing-tool placeholder through existing `sending` state or a new readonly controller property.
- `apps/agent-core/internal/chat/provider.go`
  - Extend message/tool/event types.
- `apps/agent-core/internal/chat/openai/provider.go`
  - Send tools, serialize tool messages, aggregate streaming tool_calls, and stop emitting run lifecycle.
- `apps/agent-core/internal/chat/openai/provider_test.go`
  - Assert tools request body and compact `TOOL_CALL`.
- `apps/agent-core/internal/chat/service/service.go`
  - Own run lifecycle and tool loop.
- `apps/agent-core/internal/chat/service/service_test.go`
  - Assert tool loop appends assistant tool_calls + tool role result and emits single run lifecycle.
- `apps/agent-core/internal/api/server.go`
  - Add `/v1/chat/tool-result`.
- `apps/agent-core/internal/api/server_test.go`
  - Assert endpoint routes result to waiting run and stale run returns 404.

Do not modify:

- `docs/v2/设计方案/**` unless implementation reveals a design contradiction.
- `tools/split_manifest_clips.py` and generated clip assets.
- Existing random locomotion behavior triggers except where tests need to assert they remain intact.

---

### Task 0: Preflight Baseline

**Files:**
- Read-only.

- [ ] **Step 1: Confirm branch, status, and main merge position**

Run:

```bash
pwd
git status --short --branch
git log --oneline --decorate -5
```

Expected:

```text
/Users/tian/projects/my-projects/MilesEdgeworth
## feature/phase-2-5-movement-api
```

`git status --short` must show only this plan file if the plan has already been saved. If there are other local changes, stop and inspect before coding.

- [ ] **Step 2: Configure baseline**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
```

Expected:

```text
-- Build files have been written to: /Users/tian/projects/my-projects/MilesEdgeworth/build
```

- [ ] **Step 3: Build focused baseline targets**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop PetRuntimeSmoke ChatControllerSmoke ChatStreamEventParserSmoke SkinManifestLoaderSmoke MilesAgentCore
```

Expected:

```text
[100%] Built target MilesEdgeworthDesktop
[100%] Built target PetRuntimeSmoke
[100%] Built target ChatControllerSmoke
[100%] Built target ChatStreamEventParserSmoke
[100%] Built target SkinManifestLoaderSmoke
[100%] Built target MilesAgentCore
```

- [ ] **Step 4: Run focused baseline tests**

Run:

```bash
ctest --test-dir build -R "pet_runtime_smoke|chat_controller_smoke|chat_stream_event_parser_smoke|skin_manifest_loader_smoke|check_phase_2_4_phased_animation|check_phase_2_4_2_precut_clips" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 5: Run Go baseline tests**

Run:

```bash
cd apps/agent-core && go test ./internal/chat/... ./internal/api/...
```

Expected:

```text
ok  	milesedgeworth/agent-core/internal/chat/...
ok  	milesedgeworth/agent-core/internal/api
```

- [ ] **Step 6: No commit**

This task changes nothing.

---

### Task 1: MotionController Core

**Files:**
- Create: `apps/desktop/src/pet/motion/MotionController.h`
- Create: `apps/desktop/src/pet/motion/MotionController.cpp`
- Create: `apps/desktop/tests/motion_controller_smoke.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Write the failing MotionController smoke test**

Create `apps/desktop/tests/motion_controller_smoke.cpp`:

```cpp
#include "pet/motion/MotionController.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPoint>
#include <QRect>
#include <QThread>

#include <cmath>
#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool waitFor(const std::function<bool()> &predicate, int timeoutMs = 1200)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return predicate();
}

MotionConfig testConfig()
{
    MotionConfig config;
    config.walkSpeed = 300.0;
    config.runSpeed = 600.0;
    config.snapDistance = 4.0;
    config.petScale = 1.0;
    config.petWindowSize = 20.0;
    config.availableDirections = {
        QStringLiteral("east"),
        QStringLiteral("west"),
        QStringLiteral("north"),
        QStringLiteral("south"),
        QStringLiteral("northEast"),
        QStringLiteral("northWest"),
        QStringLiteral("southEast"),
        QStringLiteral("southWest"),
    };
    return config;
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    MotionController controller;
    controller.configure(testConfig());
    controller.setScreenGeometry(QRect(0, 0, 120, 100));
    controller.setCurrentPosition(QPoint(0, 0));

    QString startedDirection;
    QString startedMode;
    QPoint lastPosition;
    bool completed = false;
    double finalX = -1.0;
    double finalY = -1.0;
    int positionChangedCount = 0;

    QObject::connect(&controller, &MotionController::started, &controller,
                     [&](const QString &direction, const QString &mode) {
                         startedDirection = direction;
                         startedMode = mode;
                     });
    QObject::connect(&controller, &MotionController::positionChanged, &controller,
                     [&](const QPoint &position) {
                         lastPosition = position;
                         ++positionChangedCount;
                     });
    QObject::connect(&controller, &MotionController::completed, &controller,
                     [&](double x, double y) {
                         completed = true;
                         finalX = x;
                         finalY = y;
                     });

    controller.moveTo(1.0, 1.0, QStringLiteral("run"));
    require(startedDirection == QStringLiteral("southEast"), "moveTo bottom-right should start southEast");
    require(startedMode == QStringLiteral("run"), "run mode should be preserved");
    require(controller.isMoving(), "controller should be moving after moveTo");
    require(waitFor([&]() { return completed; }), "moveTo should complete");
    require(positionChangedCount > 0, "moveTo should emit position changes");
    require(lastPosition == QPoint(100, 80), "reachable bottom-right should subtract pet window size");
    require(std::abs(finalX - 1.0) < 0.001, "final x percent should be 1");
    require(std::abs(finalY - 1.0) < 0.001, "final y percent should be 1");
    require(!controller.isMoving(), "controller should be idle after completion");

    completed = false;
    positionChangedCount = 0;
    controller.setCurrentPosition(QPoint(50, 40));
    controller.moveBy(1.0, -1.0, QStringLiteral("walk"));
    require(startedDirection == QStringLiteral("northEast"), "moveBy up-right should start northEast");
    require(startedMode == QStringLiteral("walk"), "walk mode should be preserved");
    require(waitFor([&]() { return completed; }), "moveBy should complete");
    require(lastPosition == QPoint(100, 0), "moveBy should clamp to reachable screen edge");
    require(controller.lastMoveWasClamped(), "clamped move should remember clamp note");

    bool interrupted = false;
    QString interruptedReason;
    QObject::connect(&controller, &MotionController::interrupted, &controller,
                     [&](const QString &reason) {
                         interrupted = true;
                         interruptedReason = reason;
                     });
    controller.setCurrentPosition(QPoint(0, 0));
    controller.moveTo(1.0, 0.0, QStringLiteral("walk"));
    require(controller.isMoving(), "controller should move before drag cancel");
    controller.cancelForDrag();
    require(interrupted, "drag cancel should emit interrupted");
    require(interruptedReason == QStringLiteral("drag"), "drag cancel reason should be drag");
    require(!controller.isMoving(), "controller should be idle after drag cancel");

    return 0;
}
```

- [ ] **Step 2: Wire the failing test target**

In `apps/desktop/CMakeLists.txt`, add these files to `DESKTOP_SOURCES`:

```cmake
    src/pet/motion/MotionController.cpp
    src/pet/motion/MotionController.h
```

Inside `if(BUILD_TESTING)`, before `PetRuntimeSmoke`, add:

```cmake
    add_executable(MotionControllerSmoke
        tests/motion_controller_smoke.cpp
        src/pet/motion/MotionController.cpp
        src/pet/motion/MotionController.h
    )

    target_include_directories(MotionControllerSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(MotionControllerSmoke
        PRIVATE
            Qt6::Core
    )

    add_test(NAME motion_controller_smoke COMMAND MotionControllerSmoke)
```

- [ ] **Step 3: Run test to verify it fails**

Run:

```bash
cmake --build build --target MotionControllerSmoke
```

Expected: build fails because `pet/motion/MotionController.h` does not exist.

- [ ] **Step 4: Add MotionController header**

Create `apps/desktop/src/pet/motion/MotionController.h`:

```cpp
#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QElapsedTimer>

struct MotionConfig
{
    double walkSpeed = 60.0;
    double runSpeed = 120.0;
    double snapDistance = 5.0;
    double petScale = 1.0;
    double petWindowSize = 120.0;
    QStringList availableDirections;
};

class MotionController : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Moving };

    explicit MotionController(QObject *parent = nullptr);

    void configure(const MotionConfig &config);
    void setScreenGeometry(const QRect &screenGeometry);
    void setCurrentPosition(const QPoint &petWindowPosition);

    void moveTo(double x, double y, const QString &mode);
    void moveBy(double dx, double dy, const QString &mode);
    void stop();
    void cancelForDrag();

    State state() const { return m_state; }
    bool isMoving() const { return m_state == State::Moving; }
    bool lastMoveWasClamped() const { return m_lastMoveWasClamped; }
    QPoint currentPosition() const { return m_currentPosition; }
    QPointF currentPercentPositionForResult() const { return currentPercentPosition(); }

signals:
    void started(const QString &movementDirection, const QString &mode);
    void positionChanged(const QPoint &newPosition);
    void directionChanged(const QString &newDirection);
    void completed(double finalX, double finalY);
    void interrupted(const QString &reason);

private:
    void tick();
    QRect reachableGeometry() const;
    QPoint targetForPercent(double x, double y, bool *clamped) const;
    QPoint targetForDeltaPercent(double dx, double dy, bool *clamped) const;
    QPoint clampToReachable(const QPointF &candidate, bool *clamped) const;
    QPointF currentPercentPosition() const;
    QString normalizedMode(const QString &mode) const;
    double speedForMode(const QString &mode) const;
    QString directionForVector(const QPointF &vector) const;
    void beginMove(const QPoint &target, const QString &mode, bool clamped);
    void finishAtTarget();
    void interruptWithReason(const QString &reason);

    MotionConfig m_config;
    QRect m_screenGeometry;
    QPoint m_currentPosition;
    QPoint m_targetPosition;
    State m_state = State::Idle;
    QTimer m_tickTimer;
    QElapsedTimer m_elapsed;
    QString m_currentDirection;
    QString m_currentMode = QStringLiteral("walk");
    bool m_lastMoveWasClamped = false;
};
```

- [ ] **Step 5: Add MotionController implementation**

Create `apps/desktop/src/pet/motion/MotionController.cpp`:

```cpp
#include "pet/motion/MotionController.h"

#include <QLineF>
#include <QtMath>
#include <QtGlobal>

namespace {
constexpr int kTickIntervalMs = 16;

double normalizedAngle(double degrees)
{
    while (degrees <= -180.0) {
        degrees += 360.0;
    }
    while (degrees > 180.0) {
        degrees -= 360.0;
    }
    return degrees;
}

QString fallbackDirection(const QStringList &directions)
{
    if (directions.contains(QStringLiteral("east"))) {
        return QStringLiteral("east");
    }
    return directions.isEmpty() ? QStringLiteral("east") : directions.constFirst();
}
} // namespace

MotionController::MotionController(QObject *parent)
    : QObject(parent)
{
    m_tickTimer.setInterval(kTickIntervalMs);
    connect(&m_tickTimer, &QTimer::timeout, this, &MotionController::tick);
}

void MotionController::configure(const MotionConfig &config)
{
    m_config = config;
    if (m_config.petScale <= 0.0) {
        m_config.petScale = 1.0;
    }
    if (m_config.petWindowSize <= 0.0) {
        m_config.petWindowSize = 120.0;
    }
    if (m_config.walkSpeed <= 0.0) {
        m_config.walkSpeed = 60.0;
    }
    if (m_config.runSpeed <= 0.0) {
        m_config.runSpeed = 120.0;
    }
    if (m_config.snapDistance <= 0.0) {
        m_config.snapDistance = 5.0;
    }
}

void MotionController::setScreenGeometry(const QRect &screenGeometry)
{
    if (screenGeometry.isValid()) {
        m_screenGeometry = screenGeometry;
    }
}

void MotionController::setCurrentPosition(const QPoint &petWindowPosition)
{
    m_currentPosition = petWindowPosition;
}

void MotionController::moveTo(double x, double y, const QString &mode)
{
    bool clamped = false;
    beginMove(targetForPercent(x, y, &clamped), normalizedMode(mode), clamped);
}

void MotionController::moveBy(double dx, double dy, const QString &mode)
{
    bool clamped = false;
    beginMove(targetForDeltaPercent(dx, dy, &clamped), normalizedMode(mode), clamped);
}

void MotionController::stop()
{
    interruptWithReason(QStringLiteral("stop"));
}

void MotionController::cancelForDrag()
{
    interruptWithReason(QStringLiteral("drag"));
}

QRect MotionController::reachableGeometry() const
{
    const QRect screen = m_screenGeometry.isValid() ? m_screenGeometry : QRect(0, 0, 1, 1);
    const int scaledWindowSize = qMax(1, qRound(m_config.petWindowSize * m_config.petScale));
    const int width = qMax(0, screen.width() - scaledWindowSize);
    const int height = qMax(0, screen.height() - scaledWindowSize);
    return QRect(screen.x(), screen.y(), width, height);
}

QPoint MotionController::targetForPercent(double x, double y, bool *clamped) const
{
    const QRect reachable = reachableGeometry();
    const QPointF candidate(
        reachable.x() + x * reachable.width(),
        reachable.y() + y * reachable.height()
    );
    return clampToReachable(candidate, clamped);
}

QPoint MotionController::targetForDeltaPercent(double dx, double dy, bool *clamped) const
{
    const QRect reachable = reachableGeometry();
    const QPointF candidate(
        m_currentPosition.x() + dx * reachable.width(),
        m_currentPosition.y() + dy * reachable.height()
    );
    return clampToReachable(candidate, clamped);
}

QPoint MotionController::clampToReachable(const QPointF &candidate, bool *clamped) const
{
    const QRect reachable = reachableGeometry();
    const double minX = reachable.x();
    const double minY = reachable.y();
    const double maxX = reachable.x() + reachable.width();
    const double maxY = reachable.y() + reachable.height();
    const double clampedX = qBound(minX, candidate.x(), maxX);
    const double clampedY = qBound(minY, candidate.y(), maxY);
    if (clamped != nullptr) {
        *clamped = !qFuzzyCompare(clampedX + 1.0, candidate.x() + 1.0)
            || !qFuzzyCompare(clampedY + 1.0, candidate.y() + 1.0);
    }
    return QPoint(qRound(clampedX), qRound(clampedY));
}

QPointF MotionController::currentPercentPosition() const
{
    const QRect reachable = reachableGeometry();
    const double x = reachable.width() > 0
        ? double(m_currentPosition.x() - reachable.x()) / double(reachable.width())
        : 0.0;
    const double y = reachable.height() > 0
        ? double(m_currentPosition.y() - reachable.y()) / double(reachable.height())
        : 0.0;
    return QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0));
}

QString MotionController::normalizedMode(const QString &mode) const
{
    const QString trimmed = mode.trimmed();
    return trimmed == QStringLiteral("run") ? QStringLiteral("run") : QStringLiteral("walk");
}

double MotionController::speedForMode(const QString &mode) const
{
    const double baseSpeed = mode == QStringLiteral("run") ? m_config.runSpeed : m_config.walkSpeed;
    return baseSpeed * m_config.petScale;
}

QString MotionController::directionForVector(const QPointF &vector) const
{
    if (qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y())) {
        return m_currentDirection.isEmpty() ? fallbackDirection(m_config.availableDirections) : m_currentDirection;
    }

    const double angle = normalizedAngle(qRadiansToDegrees(qAtan2(-vector.y(), vector.x())));
    QString direction;
    if (angle > -22.5 && angle <= 22.5) {
        direction = QStringLiteral("east");
    } else if (angle > 22.5 && angle <= 67.5) {
        direction = QStringLiteral("northEast");
    } else if (angle > 67.5 && angle <= 112.5) {
        direction = QStringLiteral("north");
    } else if (angle > 112.5 && angle <= 157.5) {
        direction = QStringLiteral("northWest");
    } else if (angle > 157.5 || angle <= -157.5) {
        direction = QStringLiteral("west");
    } else if (angle > -157.5 && angle <= -112.5) {
        direction = QStringLiteral("southWest");
    } else if (angle > -112.5 && angle <= -67.5) {
        direction = QStringLiteral("south");
    } else {
        direction = QStringLiteral("southEast");
    }

    if (m_config.availableDirections.contains(direction)) {
        return direction;
    }
    return fallbackDirection(m_config.availableDirections);
}

void MotionController::beginMove(const QPoint &target, const QString &mode, bool clamped)
{
    m_targetPosition = target;
    m_currentMode = mode;
    m_lastMoveWasClamped = clamped;

    const QString nextDirection = directionForVector(QPointF(m_targetPosition - m_currentPosition));
    const bool wasIdle = m_state == State::Idle;
    m_state = State::Moving;
    m_elapsed.restart();

    if (wasIdle) {
        m_currentDirection = nextDirection;
        emit started(m_currentDirection, m_currentMode);
    } else if (m_currentDirection != nextDirection) {
        m_currentDirection = nextDirection;
        emit directionChanged(m_currentDirection);
    }

    if (!m_tickTimer.isActive()) {
        m_tickTimer.start();
    }
    tick();
}

void MotionController::tick()
{
    if (m_state != State::Moving) {
        return;
    }

    const QPointF delta = QPointF(m_targetPosition - m_currentPosition);
    const double distance = std::hypot(delta.x(), delta.y());
    const double snapDistance = m_config.snapDistance * m_config.petScale;
    if (distance <= snapDistance) {
        finishAtTarget();
        return;
    }

    const qint64 elapsedMs = qMax<qint64>(1, m_elapsed.restart());
    const double step = speedForMode(m_currentMode) * (double(elapsedMs) / 1000.0);
    const double ratio = qMin(1.0, step / distance);
    const QPoint next(
        qRound(m_currentPosition.x() + delta.x() * ratio),
        qRound(m_currentPosition.y() + delta.y() * ratio)
    );

    const QString nextDirection = directionForVector(delta);
    if (nextDirection != m_currentDirection) {
        m_currentDirection = nextDirection;
        emit directionChanged(m_currentDirection);
    }

    if (next != m_currentPosition) {
        m_currentPosition = next;
        emit positionChanged(m_currentPosition);
    }
}

void MotionController::finishAtTarget()
{
    m_tickTimer.stop();
    m_currentPosition = m_targetPosition;
    m_state = State::Idle;
    emit positionChanged(m_currentPosition);
    const QPointF percent = currentPercentPosition();
    emit completed(percent.x(), percent.y());
}

void MotionController::interruptWithReason(const QString &reason)
{
    if (m_state != State::Moving) {
        return;
    }
    m_tickTimer.stop();
    m_state = State::Idle;
    emit interrupted(reason);
}
```

- [ ] **Step 6: Run MotionController test**

Run:

```bash
cmake --build build --target MotionControllerSmoke
ctest --test-dir build -R motion_controller_smoke --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 7: Commit**

Run:

```bash
git add apps/desktop/src/pet/motion apps/desktop/tests/motion_controller_smoke.cpp apps/desktop/CMakeLists.txt
git commit -m "feat: 添加目标点移动控制器"
```

---

### Task 2: Manifest Motion Schema

**Files:**
- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`

- [ ] **Step 1: Add failing loader assertions**

In `apps/desktop/tests/skin_manifest_loader_smoke.cpp`, after the manifest load assertion for the Miles skin, add:

```cpp
    require(std::abs(manifest.motion.walkSpeed - 60.0) < 0.001,
            "manifest motion.walkSpeed should parse");
    require(std::abs(manifest.motion.runSpeed - 120.0) < 0.001,
            "manifest motion.runSpeed should parse");
    require(std::abs(manifest.motion.snapDistance - 5.0) < 0.001,
            "manifest motion.snapDistance should parse");
```

If the file does not include `<cmath>`, add:

```cpp
#include <cmath>
```

- [ ] **Step 2: Run loader test and verify it fails**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build -R skin_manifest_loader_smoke --output-on-failure
```

Expected: build fails because `SkinManifest` has no `motion` member.

- [ ] **Step 3: Add manifest type**

In `apps/desktop/src/pet/manifest/SkinManifest.h`, after `CanvasDefinition`, add:

```cpp
struct MotionDefinition
{
    double walkSpeed = 60.0;
    double runSpeed = 120.0;
    double snapDistance = 5.0;
};
```

In `struct SkinManifest`, add:

```cpp
    MotionDefinition motion;
```

- [ ] **Step 4: Parse manifest motion block**

In `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`, inside `parseManifestDocument` after parsing `canvas`, add:

```cpp
    const QJsonObject motion = root.value("motion").toObject();
    manifest.motion.walkSpeed = motion.value("walkSpeed").toDouble(manifest.motion.walkSpeed);
    manifest.motion.runSpeed = motion.value("runSpeed").toDouble(manifest.motion.runSpeed);
    manifest.motion.snapDistance = motion.value("snapDistance").toDouble(manifest.motion.snapDistance);
    if (manifest.motion.walkSpeed <= 0.0) {
        manifest.motion.walkSpeed = 60.0;
    }
    if (manifest.motion.runSpeed <= 0.0) {
        manifest.motion.runSpeed = 120.0;
    }
    if (manifest.motion.snapDistance <= 0.0) {
        manifest.motion.snapDistance = 5.0;
    }
```

- [ ] **Step 5: Add Miles manifest defaults**

In `apps/desktop/resources/skins/miles-edgeworth/manifest.json`, add a top-level `motion` object after `canvas`:

```json
  "motion": {
    "walkSpeed": 60,
    "runSpeed": 120,
    "snapDistance": 5
  },
```

- [ ] **Step 6: Run loader test**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build -R skin_manifest_loader_smoke --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 7: Commit**

Run:

```bash
git add apps/desktop/src/pet/manifest/SkinManifest.h apps/desktop/src/pet/manifest/SkinManifestLoader.cpp apps/desktop/resources/skins/miles-edgeworth/manifest.json apps/desktop/tests/skin_manifest_loader_smoke.cpp
git commit -m "feat: 添加皮肤移动参数配置"
```

---

### Task 3: PetRuntime Motion Integration

**Files:**
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/src/pet/PetRuntimeSkin.cpp`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`

- [ ] **Step 1: Add failing runtime assertions**

In `apps/desktop/tests/pet_runtime_smoke.cpp`, add a test block near the other runtime state tests:

```cpp
    {
        PetRuntime motionRuntime;
        QPoint lastMotionPosition;
        bool motionCompleted = false;
        QVariantMap completedResult;

        QObject::connect(&motionRuntime, &PetRuntime::motionPositionChanged,
                         &motionRuntime, [&](const QPoint &position) {
                             lastMotionPosition = position;
                         });
        QObject::connect(&motionRuntime, &PetRuntime::motionCompleted,
                         &motionRuntime, [&](const QVariantMap &result) {
                             motionCompleted = true;
                             completedResult = result;
                         });

        motionRuntime.setMotionScreenGeometry(QRect(0, 0, 120, 100));
        motionRuntime.setMotionCurrentPosition(QPoint(0, 0));
        motionRuntime.requestMotion(QStringLiteral("moveTo"), 1.0, 0.0, QStringLiteral("walk"));

        require(motionRuntime.currentState() == QStringLiteral("moving"),
                "requestMotion should enter moving state without setState side effects");
        require(motionRuntime.currentActionId() == QStringLiteral("walk"),
                "requestMotion should play locomotion action");
        require(motionRuntime.currentMovementDirection() == QStringLiteral("east"),
                "requestMotion should select east movement direction");
        require(motionRuntime.currentLoopMode() == QStringLiteral("loop"),
                "target movement should override onceThenIdle locomotion to loop");
        require(!motionRuntime.currentAutoReturnToIdle(),
                "target movement locomotion must not auto return to idle after one GIF");

        require(waitFor([&]() { return motionCompleted; }, 1200),
                "target movement should complete");
        require(lastMotionPosition == QPoint(0, 0) || lastMotionPosition == QPoint(100, 0),
                "motion should emit at least one position");
        require(completedResult.value(QStringLiteral("success")).toBool(),
                "completed motion result should be successful");
        require(motionRuntime.currentState() == QStringLiteral("idle"),
                "completed motion should return runtime to idle");
        require(motionRuntime.autoMovementEnabled(),
                "completed motion should restore auto movement");
    }
```

Place this block inside the existing `main` body after the local `waitFor` helper has been declared.

- [ ] **Step 2: Run runtime test and verify it fails**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
```

Expected: build fails because `PetRuntime` has no motion API or signals.

- [ ] **Step 3: Add PetRuntime API and member**

In `apps/desktop/src/pet/PetRuntime.h`, include MotionController:

```cpp
#include "pet/motion/MotionController.h"
```

Add public methods:

```cpp
    Q_INVOKABLE void requestMotion(const QString &action, double x, double y, const QString &mode);
    Q_INVOKABLE void stopMotion();
    void cancelMotionForDrag();
    void setMotionScreenGeometry(const QRect &screenGeometry);
    void setMotionCurrentPosition(const QPoint &petWindowPosition);
```

Add signals:

```cpp
    void motionPositionChanged(const QPoint &newPosition);
    void motionCompleted(const QVariantMap &result);
    void motionInterrupted(const QVariantMap &result);
```

Add private methods:

```cpp
    void configureMotionController();
    void enterMovingState();
    void exitMovingState();
    void handleMotionStarted(const QString &movementDirection, const QString &mode);
    void handleMotionCompleted(double finalX, double finalY);
    void handleMotionInterrupted(const QString &reason);
    QVariantMap motionResult(bool success, const QString &reason, double x, double y) const;
```

Add private members:

```cpp
    MotionController m_motionController;
    QString m_currentMotionMode = QStringLiteral("walk");
    bool m_motionLoopOverride = false;
```

- [ ] **Step 4: Connect MotionController in PetRuntime constructor**

In `PetRuntime::PetRuntime`, after existing controller connections, add:

```cpp
    connect(&m_motionController, &MotionController::started,
            this, &PetRuntime::handleMotionStarted);
    connect(&m_motionController, &MotionController::positionChanged,
            this, &PetRuntime::motionPositionChanged);
    connect(&m_motionController, &MotionController::directionChanged,
            this, [this](const QString &direction) {
                playLocomotion(m_currentMotionMode, direction);
            });
    connect(&m_motionController, &MotionController::completed,
            this, &PetRuntime::handleMotionCompleted);
    connect(&m_motionController, &MotionController::interrupted,
            this, &PetRuntime::handleMotionInterrupted);
```

- [ ] **Step 5: Implement requestMotion and handlers**

In `apps/desktop/src/pet/PetRuntime.cpp`, add:

```cpp
void PetRuntime::requestMotion(const QString &action, double x, double y, const QString &mode)
{
    const QString normalizedAction = action.trimmed();
    const QString normalizedMode = mode.trimmed() == QStringLiteral("run")
        ? QStringLiteral("run")
        : QStringLiteral("walk");
    m_currentMotionMode = normalizedMode;

    if (normalizedAction == QStringLiteral("moveTo")) {
        m_motionController.moveTo(x, y, normalizedMode);
        return;
    }
    if (normalizedAction == QStringLiteral("moveBy")) {
        m_motionController.moveBy(x, y, normalizedMode);
    }
}

void PetRuntime::stopMotion()
{
    m_motionController.stop();
}

void PetRuntime::cancelMotionForDrag()
{
    m_motionController.cancelForDrag();
}

void PetRuntime::setMotionScreenGeometry(const QRect &screenGeometry)
{
    m_motionController.setScreenGeometry(screenGeometry);
}

void PetRuntime::setMotionCurrentPosition(const QPoint &petWindowPosition)
{
    m_motionController.setCurrentPosition(petWindowPosition);
}

void PetRuntime::enterMovingState()
{
    const bool stateChanged = m_currentState != QStringLiteral("moving");
    m_currentState = QStringLiteral("moving");
    m_suppressAutoIdle = true;
    m_motionLoopOverride = true;
    setAutoMovementEnabled(false);
    if (stateChanged) {
        emit currentStateChanged();
    }
}

void PetRuntime::exitMovingState()
{
    m_motionLoopOverride = false;
    m_suppressAutoIdle = false;
    setAutoMovementEnabled(true);
}

void PetRuntime::handleMotionStarted(const QString &movementDirection, const QString &mode)
{
    m_currentMotionMode = mode == QStringLiteral("run") ? QStringLiteral("run") : QStringLiteral("walk");
    enterMovingState();
    playLocomotion(m_currentMotionMode, movementDirection);
}

void PetRuntime::handleMotionCompleted(double finalX, double finalY)
{
    exitMovingState();
    returnToIdle();
    emit motionCompleted(motionResult(true, QString(), finalX, finalY));
}

void PetRuntime::handleMotionInterrupted(const QString &reason)
{
    const QPointF percent = m_motionController.currentPercentPositionForResult();
    exitMovingState();
    if (reason != QStringLiteral("drag")) {
        returnToIdle();
    }
    emit motionInterrupted(motionResult(false, reason, percent.x(), percent.y()));
}

QVariantMap PetRuntime::motionResult(bool success, const QString &reason, double x, double y) const
{
    QVariantMap position;
    position.insert(QStringLiteral("x"), x);
    position.insert(QStringLiteral("y"), y);

    QVariantMap result;
    result.insert(QStringLiteral("success"), success);
    result.insert(QStringLiteral("position"), position);
    if (!success && !reason.isEmpty()) {
        result.insert(QStringLiteral("reason"),
                      reason == QStringLiteral("drag") ? QStringLiteral("interrupted_by_user") : reason);
    }
    if (success && m_motionController.lastMoveWasClamped()) {
        result.insert(QStringLiteral("note"), QStringLiteral("clamped_to_screen_edge"));
    }
    return result;
}
```

- [ ] **Step 6: Add setAutoMovementEnabled helper**

Add a private helper:

```cpp
void PetRuntime::setAutoMovementEnabled(bool enabled)
{
    if (m_autoMovementEnabled == enabled) {
        return;
    }
    m_autoMovementEnabled = enabled;
    emit autoMovementEnabledChanged();
}
```

Declare it in private methods:

```cpp
    void setAutoMovementEnabled(bool enabled);
```

Update `toggleAutoMovementEnabled()` to call it:

```cpp
void PetRuntime::toggleAutoMovementEnabled()
{
    setAutoMovementEnabled(!m_autoMovementEnabled);
}
```

- [ ] **Step 7: Override locomotion loop during target movement**

In `PetRuntime::playLocomotion`, replace `playAction(actionId);` with:

```cpp
    if (m_motionLoopOverride && m_manifest.actions.contains(actionId)) {
        ActionDefinition action = m_manifest.actions.value(actionId);
        action.loopMode = QStringLiteral("loop");
        setCurrentAction(actionId, action);
        return;
    }

    playAction(actionId);
```

This keeps manifest `walk` / `run` as `onceThenIdle` for random movement, while target movement loops.

- [ ] **Step 8: Configure MotionController from manifest and scale**

In `PetRuntime::configureMotionController`, add:

```cpp
void PetRuntime::configureMotionController()
{
    MotionConfig config;
    config.walkSpeed = m_manifest.motion.walkSpeed;
    config.runSpeed = m_manifest.motion.runSpeed;
    config.snapDistance = m_manifest.motion.snapDistance;
    config.petScale = m_petScale > 0.0 ? m_petScale : 1.0;
    config.petWindowSize = m_manifest.canvas.windowSize;
    config.availableDirections = m_manifest.movementDirections;
    m_motionController.configure(config);
}
```

Call `configureMotionController()` from:

- `PetRuntime` constructor after `applyManifestState()` has loaded the skin.
- `applyManifestState()`.
- `setPetSize()` after `m_petScale` changes.
- `loadSkinDescriptor()` after a new manifest is accepted.

- [ ] **Step 9: Add MotionController sources to test targets**

In `apps/desktop/CMakeLists.txt`, add these two source files to `PetRuntimeSmoke`, `ChatControllerSmoke`, and `SettingsServiceSmoke` source lists:

```cmake
        src/pet/motion/MotionController.cpp
        src/pet/motion/MotionController.h
```

- [ ] **Step 10: Run runtime tests**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
ctest --test-dir build -R pet_runtime_smoke --output-on-failure
```

Expected:

```text
100% tests passed
```

- [x] **Step 11: Commit**

Run:

```bash
git add apps/desktop/src/pet/PetRuntime.h apps/desktop/src/pet/PetRuntime.cpp apps/desktop/src/pet/PetRuntimeSkin.cpp apps/desktop/src/pet/motion apps/desktop/CMakeLists.txt apps/desktop/tests/pet_runtime_smoke.cpp
git commit -m "feat: 接入运行时目标移动"
```

---

### Task 4: Window Geometry and Drag Interruption Bridge

**Files:**
- Modify: `apps/desktop/src/DesktopShellController.h`
- Modify: `apps/desktop/src/DesktopShellController.cpp`
- Modify: `apps/desktop/src/main.cpp`
- Modify: `apps/desktop/src/pet/events/PetEventBridge.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`

- [ ] **Step 1: Add DesktopShell geometry API**

In `DesktopShellController.h`, add public method:

```cpp
    QRect petMotionScreenGeometry() const;
```

In `DesktopShellController.cpp`, implement:

```cpp
QRect DesktopShellController::petMotionScreenGeometry() const
{
    QScreen *targetScreen = nullptr;
    if (m_petWindow != nullptr) {
        const QPoint petCenter(
            m_petWindow->x() + m_petWindow->width() / 2,
            m_petWindow->y() + m_petWindow->height() / 2
        );
        targetScreen = QGuiApplication::screenAt(petCenter);
        if (targetScreen == nullptr) {
            targetScreen = m_petWindow->screen();
        }
    }
    if (targetScreen == nullptr) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    return targetScreen != nullptr ? targetScreen->geometry() : QRect();
}
```

- [ ] **Step 2: Wire runtime motion to shell in main.cpp**

In `apps/desktop/src/main.cpp`, immediately after the existing `PetSurfaceWindow petSurfaceWindow(&petRuntime, &petEventBridge, &shellController, &chatController);` line, add:

```cpp
    auto syncMotionEnvironment = [&shellController, &petRuntime]() {
        petRuntime.setMotionScreenGeometry(shellController.petMotionScreenGeometry());
        petRuntime.setMotionCurrentPosition(QPoint(shellController.petWindowX(), shellController.petWindowY()));
    };
    QObject::connect(&shellController, &DesktopShellController::petWindowGeometryChanged,
                     &petRuntime, syncMotionEnvironment);
    QObject::connect(&petRuntime, &PetRuntime::motionPositionChanged,
                     &shellController, [&shellController](const QPoint &position) {
                         shellController.movePetWindowTo(position.x(), position.y());
                     });
```

Inside the existing startup lambda whose capture list is `[&petSurfaceWindow, &shellController, &petRuntime]`, immediately after `shellController.placePetWindowForStartup(petRuntime.petScale());`, add:

```cpp
        petRuntime.setMotionScreenGeometry(shellController.petMotionScreenGeometry());
        petRuntime.setMotionCurrentPosition(QPoint(shellController.petWindowX(), shellController.petWindowY()));
```

- [ ] **Step 3: Cancel motion on drag start**

In `PetEventBridge::submitDragStarted`, after the null runtime guard and before reading the snapshot, add:

```cpp
    m_runtime->cancelMotionForDrag();
```

This is safe because `cancelMotionForDrag()` is a no-op when not moving.

- [ ] **Step 4: Run focused Qt build**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop PetRuntimeSmoke
ctest --test-dir build -R pet_runtime_smoke --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 5: Commit**

Run:

```bash
git add apps/desktop/src/DesktopShellController.h apps/desktop/src/DesktopShellController.cpp apps/desktop/src/main.cpp apps/desktop/src/pet/events/PetEventBridge.cpp apps/desktop/tests/pet_runtime_smoke.cpp
git commit -m "feat: 连接目标移动窗口桥接"
```

---

### Task 5: Go Tool Use Skeleton and OpenAI Provider Aggregation

**Files:**
- Create: `apps/agent-core/internal/chat/service/tools.go`
- Modify: `apps/agent-core/internal/chat/provider.go`
- Modify: `apps/agent-core/internal/chat/openai/provider.go`
- Modify: `apps/agent-core/internal/chat/openai/provider_test.go`
- Modify: `apps/agent-core/internal/chat/service/service.go`
- Modify: `apps/agent-core/internal/chat/service/service_test.go`

- [x] **Step 1: Extend chat model types**

In `apps/agent-core/internal/chat/provider.go`, replace `Message`, extend `ChatParams`, and extend `StreamEvent`:

```go
type ToolDefinition struct {
	Name        string          `json:"name"`
	Description string          `json:"description"`
	Parameters  json.RawMessage `json:"parameters"`
}

type ToolCallFunction struct {
	Name      string `json:"name"`
	Arguments string `json:"arguments"`
}

type ToolCall struct {
	ID       string           `json:"id"`
	Type     string           `json:"type"`
	Function ToolCallFunction `json:"function"`
}

type Message struct {
	Role       string     `json:"role"`
	Content    string     `json:"content"`
	ToolCalls  []ToolCall `json:"tool_calls,omitempty"`
	ToolCallID string     `json:"tool_call_id,omitempty"`
}
```

Add `Tools` and `Continuation` to `ChatParams`:

```go
	Tools        []ToolDefinition
	Continuation bool
```

Add fields to `StreamEvent`:

```go
	ToolCallID string `json:"toolCallId,omitempty"`
	ToolName   string `json:"toolName,omitempty"`
	ToolArgs   string `json:"toolArgs,omitempty"`
```

Add the missing import:

```go
import (
	"context"
	"encoding/json"
)
```

- [x] **Step 2: Add tool definitions and registry**

Create `apps/agent-core/internal/chat/service/tools.go`:

```go
package service

import (
	"context"
	"encoding/json"
	"sync"
	"time"

	"milesedgeworth/agent-core/internal/chat"
)

const (
	MaxToolCallsPerRun    = 3
	ToolResultTimeout    = 120 * time.Second
	PetMotionToolName     = "pet_motion"
	ToolCallEventType     = "TOOL_CALL"
)

type ToolResult struct {
	RunID      string          `json:"runId"`
	ToolCallID string          `json:"toolCallId"`
	Result     json.RawMessage `json:"result"`
}

type toolWaiter struct {
	ch chan ToolResult
}

type toolRegistry struct {
	values sync.Map
}

func (r *toolRegistry) register(runID string) chan ToolResult {
	waiter := &toolWaiter{ch: make(chan ToolResult, 1)}
	r.values.Store(runID, waiter)
	return waiter.ch
}

func (r *toolRegistry) unregister(runID string) {
	r.values.Delete(runID)
}

func (r *toolRegistry) submit(result ToolResult) bool {
	value, ok := r.values.Load(result.RunID)
	if !ok {
		return false
	}
	waiter := value.(*toolWaiter)
	select {
	case waiter.ch <- result:
		return true
	default:
		return false
	}
}

func waitForToolResult(ctx context.Context, ch <-chan ToolResult) (ToolResult, bool) {
	timer := time.NewTimer(ToolResultTimeout)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return ToolResult{}, false
	case result := <-ch:
		return result, true
	case <-timer.C:
		return ToolResult{
			Result: json.RawMessage(`{"success":false,"reason":"timeout"}`),
		}, true
	}
}

var PetMotionTool = chat.ToolDefinition{
	Name:        PetMotionToolName,
	Description: "控制桌宠移动。moveTo 移到屏幕可达区域百分比坐标，moveBy 相对当前位置移动。",
	Parameters: json.RawMessage(`{"type":"object","properties":{"action":{"type":"string","enum":["moveTo","moveBy"],"description":"moveTo: 移到屏幕可达区域百分比坐标; moveBy: 相对当前位置移动"},"x":{"type":"number","description":"moveTo: 可达区域百分比 x; moveBy: 相对位移百分比"},"y":{"type":"number","description":"moveTo: 可达区域百分比 y; moveBy: 相对位移百分比"},"mode":{"type":"string","enum":["walk","run"],"description":"移动速度模式，默认 walk"}},"required":["action","x","y"]}`),
}

func availableTools() []chat.ToolDefinition {
	return []chat.ToolDefinition{PetMotionTool}
}
```

- [x] **Step 3: Update Service structure**

In `apps/agent-core/internal/chat/service/service.go`, add registry:

```go
	tools toolRegistry
```

Update `New`:

```go
	return &Service{
		store:    store,
		provider: provider,
		catalog:  catalog,
		modelID:  modelID,
	}
```

No explicit registry initialization is needed because `sync.Map` is zero-value ready.

Add public submit method:

```go
func (s *Service) SubmitToolResult(result ToolResult) bool {
	return s.tools.submit(result)
}
```

- [x] **Step 4: Write failing service tool-loop test**

In `apps/agent-core/internal/chat/service/service_test.go`, extend `fakeProvider`:

```go
	streamParamsList []chat.ChatParams
	streamEventLists [][]chat.StreamEvent
```

Replace its `StreamChat` method body with:

```go
	p.streamCalls++
	p.streamParams = params
	p.streamParamsList = append(p.streamParamsList, params)
	if p.streamErr != nil {
		return nil, p.streamErr
	}
	if len(p.streamEventLists) >= p.streamCalls {
		events := make(chan chat.StreamEvent, len(p.streamEventLists[p.streamCalls-1]))
		for _, event := range p.streamEventLists[p.streamCalls-1] {
			events <- event
		}
		close(events)
		return events, nil
	}
	if p.streamEvents != nil {
		return p.streamEvents, nil
	}
	events := make(chan chat.StreamEvent)
	close(events)
	return events, nil
```

Add test:

```go
func TestStreamChatToolLoopEmitsSingleRunLifecycle(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	appendMessage(t, s, conv.ID, store.RoleUser, "走到右边。")

	provider := &fakeProvider{
		streamEventLists: [][]chat.StreamEvent{
			{
				{Type: "TOOL_CALL", RunID: "run-1", ToolCallID: "tc-1", ToolName: "pet_motion", ToolArgs: `{"action":"moveTo","x":1,"y":0.5}`},
			},
			{
				{Type: "TEXT_MESSAGE_START", RunID: "run-1", MessageID: "msg-2", Role: "assistant"},
				{Type: "TEXT_MESSAGE_CONTENT", RunID: "run-1", MessageID: "msg-2", Delta: "到了。"},
				{Type: "TEXT_MESSAGE_END", RunID: "run-1", MessageID: "msg-2"},
			},
		},
	}
	service := newTestService(s, provider, 8192)

	events, err := service.StreamChat(context.Background(), BuildRequest{
		ConversationID: conv.ID,
		RunID:          "run-1",
		MessageID:      "msg-1",
	})
	if err != nil {
		t.Fatal(err)
	}

	var got []chat.StreamEvent
	for event := range events {
		got = append(got, event)
		if event.Type == "TOOL_CALL" {
			ok := service.SubmitToolResult(ToolResult{
				RunID:      "run-1",
				ToolCallID: "tc-1",
				Result:     json.RawMessage(`{"success":true,"position":{"x":1,"y":0.5}}`),
			})
			if !ok {
				t.Fatal("SubmitToolResult should find waiting run")
			}
		}
	}

	if provider.streamCalls != 2 {
		t.Fatalf("stream calls = %d, want 2", provider.streamCalls)
	}
	if len(provider.streamParamsList[0].Tools) != 1 || provider.streamParamsList[0].Tools[0].Name != "pet_motion" {
		t.Fatalf("first provider call should include pet_motion tool: %+v", provider.streamParamsList[0].Tools)
	}
	secondMessages := provider.streamParamsList[1].Messages
	if len(secondMessages) < 2 {
		t.Fatalf("second provider call missing tool context: %+v", secondMessages)
	}
	if secondMessages[len(secondMessages)-2].Role != "assistant" || len(secondMessages[len(secondMessages)-2].ToolCalls) != 1 {
		t.Fatalf("second provider call missing assistant tool_calls message: %+v", secondMessages)
	}
	if secondMessages[len(secondMessages)-1].Role != "tool" || secondMessages[len(secondMessages)-1].ToolCallID != "tc-1" {
		t.Fatalf("second provider call missing tool result message: %+v", secondMessages)
	}

	runStarted := 0
	runFinished := 0
	for _, event := range got {
		if event.Type == "RUN_STARTED" {
			runStarted++
		}
		if event.Type == "RUN_FINISHED" {
			runFinished++
		}
	}
	if runStarted != 1 || runFinished != 1 {
		t.Fatalf("run lifecycle = started %d finished %d, want 1/1; events=%+v", runStarted, runFinished, got)
	}
}
```

Add imports:

```go
	"encoding/json"
```

- [x] **Step 5: Implement Service tool loop**

Replace `Service.StreamChat` with a version that:

1. Emits `RUN_STARTED` and `miles.pet.lifecycle` once through the returned channel.
2. Calls provider with `Tools: availableTools()`.
3. Forwards content events.
4. On `TOOL_CALL`, registers a waiter, emits the tool event, waits for result, unregisters, appends assistant/tool messages, increments `toolCalls`, then loops with `Continuation: true`.
5. Emits `RUN_FINISHED` once when a provider call completes without a tool call.

Use this helper inside `service.go`:

```go
func send(ctx context.Context, events chan<- chat.StreamEvent, event chat.StreamEvent) bool {
	select {
	case <-ctx.Done():
		return false
	case events <- event:
		return true
	}
}
```

Use this message append logic:

```go
func appendToolMessages(messages []chat.Message, toolCall chat.StreamEvent, result ToolResult) []chat.Message {
	return append(messages,
		chat.Message{
			Role: "assistant",
			ToolCalls: []chat.ToolCall{
				{
					ID:   toolCall.ToolCallID,
					Type: "function",
					Function: chat.ToolCallFunction{
						Name:      toolCall.ToolName,
						Arguments: toolCall.ToolArgs,
					},
				},
			},
		},
		chat.Message{
			Role:       "tool",
			Content:    string(result.Result),
			ToolCallID: toolCall.ToolCallID,
		},
	)
}
```

- [x] **Step 6: Update OpenAI provider request types**

In `apps/agent-core/internal/chat/openai/provider.go`, replace `chatMessage` with:

```go
type chatToolDefinition struct {
	Type     string `json:"type"`
	Function struct {
		Name        string          `json:"name"`
		Description string          `json:"description"`
		Parameters  json.RawMessage `json:"parameters"`
	} `json:"function"`
}

type chatMessage struct {
	Role       string          `json:"role"`
	Content    string          `json:"content,omitempty"`
	ToolCalls  []chat.ToolCall `json:"tool_calls,omitempty"`
	ToolCallID string          `json:"tool_call_id,omitempty"`
}
```

Extend request:

```go
	Tools []chatToolDefinition `json:"tools,omitempty"`
```

Update `makeChatMessages`:

```go
func makeChatMessages(messages []chat.Message) []chatMessage {
	out := make([]chatMessage, 0, len(messages))
	for _, message := range messages {
		out = append(out, chatMessage{
			Role:       message.Role,
			Content:    message.Content,
			ToolCalls:  message.ToolCalls,
			ToolCallID: message.ToolCallID,
		})
	}
	return out
}
```

Add:

```go
func makeChatTools(tools []chat.ToolDefinition) []chatToolDefinition {
	out := make([]chatToolDefinition, 0, len(tools))
	for _, tool := range tools {
		var item chatToolDefinition
		item.Type = "function"
		item.Function.Name = tool.Name
		item.Function.Description = tool.Description
		item.Function.Parameters = tool.Parameters
		out = append(out, item)
	}
	return out
}
```

Set `Tools: makeChatTools(params.Tools)` in streaming request body.

- [x] **Step 7: Stop provider-owned run lifecycle**

In `Provider.pipe`, remove sends for:

- `RUN_STARTED`
- `miles.pet.lifecycle`
- `RUN_FINISHED`

Keep `TEXT_MESSAGE_START`, `TEXT_MESSAGE_CONTENT`, `TEXT_MESSAGE_END`, `CUSTOM expression`, `RUN_ERROR`, and `TOOL_CALL`.

If `params.Continuation` is true, still emit a fresh `TEXT_MESSAGE_START` with the current `MessageID`.

- [x] **Step 8: Aggregate OpenAI streaming tool_calls**

Extend `chatCompletionStreamChunk`:

```go
type chatCompletionStreamChunk struct {
	Choices []struct {
		Delta struct {
			Content   string `json:"content"`
			ToolCalls []struct {
				Index int    `json:"index"`
				ID    string `json:"id"`
				Type  string `json:"type"`
				Function struct {
					Name      string `json:"name"`
					Arguments string `json:"arguments"`
				} `json:"function"`
			} `json:"tool_calls"`
		} `json:"delta"`
		FinishReason string `json:"finish_reason"`
	} `json:"choices"`
}
```

Inside `pipe`, maintain an accumulator:

```go
type pendingToolCall struct {
	id        string
	name      strings.Builder
	arguments strings.Builder
}
pendingTools := map[int]*pendingToolCall{}
```

For each streamed tool delta:

```go
item := pendingTools[tool.Index]
if item == nil {
	item = &pendingToolCall{}
	pendingTools[tool.Index] = item
}
if tool.ID != "" {
	item.id = tool.ID
}
if tool.Function.Name != "" {
	item.name.WriteString(tool.Function.Name)
}
if tool.Function.Arguments != "" {
	item.arguments.WriteString(tool.Function.Arguments)
}
```

After scanner loop and before `TEXT_MESSAGE_END`, if `len(pendingTools) > 0`, emit one compact `TOOL_CALL` per sorted index:

```go
send(ctx, events, chat.StreamEvent{
	Type:       "TOOL_CALL",
	RunID:      runID,
	ToolCallID: item.id,
	ToolName:   item.name.String(),
	ToolArgs:   item.arguments.String(),
})
```

Then return without emitting `TEXT_MESSAGE_END` if no text content started. If text content started before tool call, flush parser and emit `TEXT_MESSAGE_END` before the `TOOL_CALL`.

- [x] **Step 9: Update provider tests**

In `apps/agent-core/internal/chat/openai/provider_test.go`, update `TestStreamChatHappyPath` to expect no provider-level `RUN_STARTED` / `RUN_FINISHED`. Add a new test:

```go
func TestStreamChatAggregatesToolCall(t *testing.T) {
	var requestBody map[string]any
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if err := json.NewDecoder(r.Body).Decode(&requestBody); err != nil {
			t.Fatalf("decode request: %v", err)
		}
		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		flusher := w.(http.Flusher)
		for _, c := range []string{
			`{"choices":[{"delta":{"tool_calls":[{"index":0,"id":"tc-1","type":"function","function":{"name":"pet_","arguments":"{\"action\":\"move"}}]}}]}`,
			`{"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"name":"motion","arguments":"To\",\"x\":1,\"y\":0.5}"}}]},"finish_reason":"tool_calls"}]}`,
			`[DONE]`,
		} {
			fmt.Fprintf(w, "data: %s\n\n", c)
			flusher.Flush()
		}
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "test-model", nil, nil)
	events, err := p.StreamChat(context.Background(), chat.ChatParams{
		RunID:     "run-1",
		MessageID: "msg-1",
		Messages: []chat.Message{{Role: "user", Content: "move"}},
		Tools: []chat.ToolDefinition{{
			Name:        "pet_motion",
			Description: "move pet",
			Parameters:  json.RawMessage(`{"type":"object"}`),
		}},
	})
	if err != nil {
		t.Fatal(err)
	}
	var got []chat.StreamEvent
	for event := range events {
		got = append(got, event)
	}
	if len(got) != 1 || got[0].Type != "TOOL_CALL" {
		t.Fatalf("events = %+v, want one TOOL_CALL", got)
	}
	if got[0].ToolCallID != "tc-1" || got[0].ToolName != "pet_motion" {
		t.Fatalf("tool call fields = %+v", got[0])
	}
	if got[0].ToolArgs != `{"action":"moveTo","x":1,"y":0.5}` {
		t.Fatalf("tool args = %q", got[0].ToolArgs)
	}
	if _, ok := requestBody["tools"]; !ok {
		t.Fatalf("request body missing tools: %+v", requestBody)
	}
}
```

- [x] **Step 10: Run Go chat tests**

Run:

```bash
cd apps/agent-core && go test ./internal/chat/...
```

Expected:

```text
ok  	milesedgeworth/agent-core/internal/chat/...
```

- [ ] **Step 11: Commit**

Run:

```bash
git add apps/agent-core/internal/chat
git commit -m "feat: 添加 sidecar 工具调用循环"
```

---

### Task 6: Go Tool Result API Endpoint

**Files:**
- Modify: `apps/agent-core/internal/api/server.go`
- Modify: `apps/agent-core/internal/api/server_test.go`

- [x] **Step 1: Add failing API route test**

In `apps/agent-core/internal/api/server_test.go`, add:

```go
func TestToolResultEndpointReturns404ForStaleRun(t *testing.T) {
	s := openTestStore(t)
	server := NewServer(s, chatservice.New(s, nil, nil, "test-model"), "test")

	body := strings.NewReader(`{"runId":"missing-run","toolCallId":"tc-1","result":{"success":true}}`)
	req := httptest.NewRequest(http.MethodPost, "/v1/chat/tool-result", body)
	res := httptest.NewRecorder()

	server.Routes().ServeHTTP(res, req)

	if res.Code != http.StatusNotFound {
		t.Fatalf("status = %d, want 404; body=%s", res.Code, res.Body.String())
	}
}
```

Add imports if absent:

```go
	"net/http"
	"net/http/httptest"
	"strings"
```

- [x] **Step 2: Register route**

In `Server.Routes`, add:

```go
	mux.HandleFunc("/v1/chat/tool-result", s.handleChatToolResult)
```

- [x] **Step 3: Implement handler**

In `apps/agent-core/internal/api/server.go`, add:

```go
func (s *Server) handleChatToolResult(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeMethodNotAllowed(w, http.MethodPost)
		return
	}

	r.Body = http.MaxBytesReader(w, r.Body, MaxRequestBodyBytes)
	var req struct {
		RunID      string          `json:"runId"`
		ToolCallID string          `json:"toolCallId"`
		Result     json.RawMessage `json:"result"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]any{"error": "invalid JSON"})
		return
	}
	req.RunID = strings.TrimSpace(req.RunID)
	req.ToolCallID = strings.TrimSpace(req.ToolCallID)
	if req.RunID == "" || req.ToolCallID == "" || len(req.Result) == 0 {
		writeJSON(w, http.StatusBadRequest, map[string]any{"error": "runId, toolCallId, and result are required"})
		return
	}
	if !s.chatService.SubmitToolResult(chatservice.ToolResult{
		RunID:      req.RunID,
		ToolCallID: req.ToolCallID,
		Result:     req.Result,
	}) {
		writeJSON(w, http.StatusNotFound, map[string]any{"error": "tool result target not found"})
		return
	}
	writeJSON(w, http.StatusAccepted, map[string]any{"ok": true})
}
```

- [x] **Step 4: Run API tests**

Run:

```bash
cd apps/agent-core && go test ./internal/api/...
```

Expected:

```text
ok  	milesedgeworth/agent-core/internal/api
```

- [ ] **Step 5: Commit**

Run:

```bash
git add apps/agent-core/internal/api
git commit -m "feat: 添加工具结果回传接口"
```

---

### Task 7: Qt TOOL_CALL Parsing and ChatController Execution

**Files:**
- Modify: `apps/desktop/src/chat/ChatStreamEvent.h`
- Modify: `apps/desktop/src/chat/ChatStreamEvent.cpp`
- Modify: `apps/desktop/tests/chat_stream_event_parser_smoke.cpp`
- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`
- Modify: `apps/desktop/qml/ChatComposer.qml`

- [x] **Step 1: Add failing parser assertions**

In `apps/desktop/tests/chat_stream_event_parser_smoke.cpp`, before malformed JSON tests, add:

```cpp
    events = parser.ingest(
        "data: {\"type\":\"TOOL_CALL\",\"runId\":\"run-1\",\"toolCallId\":\"tc-1\","
        "\"toolName\":\"pet_motion\",\"toolArgs\":\"{\\\"action\\\":\\\"moveTo\\\",\\\"x\\\":1,\\\"y\\\":0.5}\"}\n\n"
    );
    require(events.size() == 1, "tool call event should parse");
    require(events[0].type == "TOOL_CALL", "tool call type should parse");
    require(events[0].toolCallId == "tc-1", "toolCallId should parse");
    require(events[0].toolName == "pet_motion", "toolName should parse");
    require(events[0].toolArgs.contains("\"moveTo\""), "toolArgs should parse");
```

- [x] **Step 2: Add parser fields**

In `ChatStreamEvent.h`, add:

```cpp
    QString toolCallId;
    QString toolName;
    QString toolArgs;
```

In `eventFromObject`, add:

```cpp
    event.toolCallId = object.value("toolCallId").toString();
    event.toolName = object.value("toolName").toString();
    event.toolArgs = object.value("toolArgs").toString();
```

- [x] **Step 3: Run parser test**

Run:

```bash
cmake --build build --target ChatStreamEventParserSmoke
ctest --test-dir build -R chat_stream_event_parser_smoke --output-on-failure
```

Expected:

```text
100% tests passed
```

- [x] **Step 4: Add ChatController state and helpers**

In `ChatController::ChatPhase`, add:

```cpp
        EXECUTING_TOOL,
```

Add private struct:

```cpp
    struct PendingToolCall
    {
        QString runId;
        QString toolCallId;
        QString toolName;
        QString toolArgs;
    };
```

Add helpers:

```cpp
    void handleToolCall(const ChatStreamEvent &event);
    void executeToolCallAfterCleanFinish();
    void executePetMotionTool(const PendingToolCall &toolCall);
    void postToolResult(const QString &runId, const QString &toolCallId, const QVariantMap &result);
    QVariantMap invalidToolResult(const QString &error) const;
    bool parsePetMotionArgs(const QString &toolArgs, QString *action, double *x, double *y, QString *mode) const;
    void handleMotionToolCompleted(const QVariantMap &result);
    void handleMotionToolInterrupted(const QVariantMap &result);
    void handleMotionToolTimeout();
```

Add members:

```cpp
    PendingToolCall m_pendingToolCall;
    QTimer m_motionToolTimeout;
    static constexpr int kMotionToolTimeoutMs = 90000;
```

- [x] **Step 5: Wire motion signals and timeout**

In `ChatController` constructor, add:

```cpp
    m_motionToolTimeout.setSingleShot(true);
    connect(&m_motionToolTimeout, &QTimer::timeout,
            this, &ChatController::handleMotionToolTimeout);
```

Inside the existing `if (m_runtime != nullptr)` block, add:

```cpp
        connect(m_runtime, &PetRuntime::motionCompleted,
                this, &ChatController::handleMotionToolCompleted);
        connect(m_runtime, &PetRuntime::motionInterrupted,
                this, &ChatController::handleMotionToolInterrupted);
```

- [x] **Step 6: Handle TOOL_CALL in applyStreamEvent**

In `ChatController::applyStreamEvent`, before `RUN_FINISHED`, add:

```cpp
    if (event.type == QStringLiteral("TOOL_CALL")) {
        handleToolCall(event);
        return;
    }
```

Implement `handleToolCall`:

```cpp
void ChatController::handleToolCall(const ChatStreamEvent &event)
{
    PendingToolCall toolCall;
    toolCall.runId = event.runId.isEmpty() ? m_activeRunId : event.runId;
    toolCall.toolCallId = event.toolCallId;
    toolCall.toolName = event.toolName;
    toolCall.toolArgs = event.toolArgs;

    if (toolCall.toolCallId.isEmpty()) {
        postToolResult(toolCall.runId, toolCall.toolCallId, invalidToolResult(QStringLiteral("missing toolCallId")));
        return;
    }

    m_pendingToolCall = toolCall;

    if (m_phase == ChatPhase::BUFFERING_FOR_START) {
        m_startTimeout.stop();
        requestCleanFinishForStream(m_currentStreamId, m_asyncGeneration, m_cleanFinishRequestId++);
        executeToolCallAfterCleanFinish();
        return;
    }

    if (m_phase == ChatPhase::STREAMING || m_phase == ChatPhase::GATED || m_phase == ChatPhase::WAITING_FOR_ANIMATION_END) {
        m_streamFinished = false;
        requestFinishCleanFinishIfPacerEmpty();
        if (m_pacerEmpty && m_animationReady) {
            executeToolCallAfterCleanFinish();
        }
        return;
    }

    postToolResult(toolCall.runId, toolCall.toolCallId, invalidToolResult(QStringLiteral("tool call received in invalid state")));
}
```

After implementing, simplify the BUFFERING path if it double-requests cleanFinish. The final behavior must be: stop start timeout, request cleanFinish, execute tool only from the cleanFinish callback.

- [x] **Step 7: Execute pet_motion**

Implement:

```cpp
bool ChatController::parsePetMotionArgs(const QString &toolArgs, QString *action, double *x, double *y, QString *mode) const
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(toolArgs.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    const QJsonObject object = document.object();
    const QString parsedAction = object.value("action").toString();
    if (parsedAction != QStringLiteral("moveTo") && parsedAction != QStringLiteral("moveBy")) {
        return false;
    }
    if (!object.value("x").isDouble() || !object.value("y").isDouble()) {
        return false;
    }
    *action = parsedAction;
    *x = object.value("x").toDouble();
    *y = object.value("y").toDouble();
    *mode = object.value("mode").toString(QStringLiteral("walk")) == QStringLiteral("run")
        ? QStringLiteral("run")
        : QStringLiteral("walk");
    return true;
}

void ChatController::executePetMotionTool(const PendingToolCall &toolCall)
{
    QString action;
    QString mode;
    double x = 0.0;
    double y = 0.0;
    if (toolCall.toolName != QStringLiteral("pet_motion")
            || !parsePetMotionArgs(toolCall.toolArgs, &action, &x, &y, &mode)
            || m_runtime == nullptr) {
        postToolResult(toolCall.runId, toolCall.toolCallId, invalidToolResult(QStringLiteral("invalid pet_motion request")));
        transitionTo(ChatPhase::BUFFERING_FOR_START);
        return;
    }

    transitionTo(ChatPhase::EXECUTING_TOOL);
    setStatusText(QStringLiteral("Miles 正在移动…"));
    m_motionToolTimeout.start(kMotionToolTimeoutMs);
    m_runtime->requestMotion(action, x, y, mode);
}
```

Implement:

```cpp
void ChatController::executeToolCallAfterCleanFinish()
{
    executePetMotionTool(m_pendingToolCall);
}
```

- [x] **Step 8: POST tool result**

Add URL constant:

```cpp
constexpr auto kToolResultUrl = "http://127.0.0.1:39710/v1/chat/tool-result";
```

Implement:

```cpp
QVariantMap ChatController::invalidToolResult(const QString &error) const
{
    QVariantMap result;
    result.insert(QStringLiteral("success"), false);
    result.insert(QStringLiteral("error"), error);
    return result;
}

void ChatController::postToolResult(const QString &runId, const QString &toolCallId, const QVariantMap &result)
{
    QJsonObject body;
    body.insert(QStringLiteral("runId"), runId);
    body.insert(QStringLiteral("toolCallId"), toolCallId);
    body.insert(QStringLiteral("result"), QJsonObject::fromVariantMap(result));

    QNetworkRequest request(QUrl(QString::fromLatin1(kToolResultUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        reply->deleteLater();
    });
}
```

- [x] **Step 9: Finish motion result paths**

Implement:

```cpp
void ChatController::handleMotionToolCompleted(const QVariantMap &result)
{
    if (m_phase != ChatPhase::EXECUTING_TOOL) {
        return;
    }
    m_motionToolTimeout.stop();
    postToolResult(m_pendingToolCall.runId, m_pendingToolCall.toolCallId, result);
    transitionTo(ChatPhase::BUFFERING_FOR_START);
    m_startTimeout.start(kStartTimeoutMs);
}

void ChatController::handleMotionToolInterrupted(const QVariantMap &result)
{
    if (m_phase != ChatPhase::EXECUTING_TOOL) {
        return;
    }
    m_motionToolTimeout.stop();
    postToolResult(m_pendingToolCall.runId, m_pendingToolCall.toolCallId, result);
    transitionTo(ChatPhase::BUFFERING_FOR_START);
    m_startTimeout.start(kStartTimeoutMs);
}

void ChatController::handleMotionToolTimeout()
{
    if (m_phase != ChatPhase::EXECUTING_TOOL || m_runtime == nullptr) {
        return;
    }
    m_runtime->stopMotion();
}
```

In `cancelCurrentReply`, add:

```cpp
    m_motionToolTimeout.stop();
    if (m_phase == ChatPhase::EXECUTING_TOOL && m_runtime != nullptr) {
        m_runtime->stopMotion();
    }
```

Do not POST a tool result on explicit user cancellation.

- [x] **Step 10: Update ChatComposer placeholder**

Update the existing `ChatComposer.qml` placeholder binding to include:

```qml
placeholderText: App.ChatController.statusText === "Miles 正在移动…" ? "Miles 正在移动…" : "输入消息..."
```

Keep the existing placeholder property name if it is not `placeholderText`; only replace the expression value.

- [x] **Step 11: Run Qt chat tests**

Run:

```bash
cmake --build build --target ChatStreamEventParserSmoke ChatControllerSmoke
ctest --test-dir build -R "chat_stream_event_parser_smoke|chat_controller_smoke" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [x] **Step 12: Commit**

Run:

```bash
git add apps/desktop/src/chat apps/desktop/tests/chat_stream_event_parser_smoke.cpp apps/desktop/tests/chat_controller_smoke.cpp apps/desktop/qml/ChatComposer.qml
git commit -m "feat: 接入聊天工具调用执行"
```

---

### Task 8: Phase 2.5 Static Contract Check

**Files:**
- Create: `tests/check_phase_2_5_movement_tool.py`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Add contract check script**

Create `tests/check_phase_2_5_movement_tool.py`:

```python
#!/usr/bin/env python3
"""Check Phase 2.5 movement and tool-use contracts."""

from __future__ import annotations

import json
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
    root_cmake = read("CMakeLists.txt")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    motion_h = read("apps/desktop/src/pet/motion/MotionController.h")
    motion_cpp = read("apps/desktop/src/pet/motion/MotionController.cpp")
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    bridge_cpp = read("apps/desktop/src/pet/events/PetEventBridge.cpp")
    main_cpp = read("apps/desktop/src/main.cpp")
    stream_h = read("apps/desktop/src/chat/ChatStreamEvent.h")
    stream_cpp = read("apps/desktop/src/chat/ChatStreamEvent.cpp")
    chat_h = read("apps/desktop/src/chat/ChatController.h")
    chat_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    provider_go = read("apps/agent-core/internal/chat/provider.go")
    openai_go = read("apps/agent-core/internal/chat/openai/provider.go")
    service_go = read("apps/agent-core/internal/chat/service/service.go")
    tools_go = read("apps/agent-core/internal/chat/service/tools.go")
    api_go = read("apps/agent-core/internal/api/server.go")

    require("check_phase_2_5_movement_tool" in root_cmake,
            "root CMake must register Phase 2.5 contract check")
    require("MotionControllerSmoke" in desktop_cmake,
            "desktop CMake must register MotionControllerSmoke")
    require("src/pet/motion/MotionController.cpp" in desktop_cmake,
            "desktop targets must compile MotionController")

    motion = manifest.get("motion")
    require(isinstance(motion, dict), "Miles manifest must declare top-level motion")
    require(motion.get("walkSpeed") == 60, "motion.walkSpeed must be 60")
    require(motion.get("runSpeed") == 120, "motion.runSpeed must be 120")
    require(motion.get("snapDistance") == 5, "motion.snapDistance must be 5")

    for token in [
        "void moveTo(double x, double y, const QString &mode);",
        "void moveBy(double dx, double dy, const QString &mode);",
        "void cancelForDrag();",
        "QElapsedTimer",
        "directionForVector",
        "lastMoveWasClamped",
    ]:
        require(token in motion_h + motion_cpp, f"MotionController missing {token}")

    for token in [
        "requestMotion",
        "enterMovingState",
        "exitMovingState",
        "m_motionLoopOverride",
        "motionPositionChanged",
        "motionCompleted",
        "motionInterrupted",
        "setAutoMovementEnabled(false)",
        "setAutoMovementEnabled(true)",
    ]:
        require(token in runtime_h + runtime_cpp, f"PetRuntime motion bridge missing {token}")
    require("setState(QStringLiteral(\"moving\"))" not in runtime_cpp,
            "moving state must not use setState side-effect path")
    require("playLocomotion" in runtime_cpp and "QStringLiteral(\"loop\")" in runtime_cpp,
            "target locomotion must override loop mode")

    require("cancelMotionForDrag" in bridge_cpp,
            "drag start must cancel active target motion")
    require("motionPositionChanged" in main_cpp and "movePetWindowTo" in main_cpp,
            "main must connect motion positions to DesktopShellController")
    require("petMotionScreenGeometry" in main_cpp,
            "main must inject screen geometry into PetRuntime")

    for token in ["toolCallId", "toolName", "toolArgs"]:
        require(token in stream_h and token in stream_cpp,
                f"ChatStreamEvent must parse {token}")
    for token in [
        "EXECUTING_TOOL",
        "handleToolCall",
        "postToolResult",
        "kToolResultUrl",
        "kMotionToolTimeoutMs",
        "requestMotion",
        "Miles 正在移动",
    ]:
        require(token in chat_h + chat_cpp,
                f"ChatController tool execution missing {token}")

    for token in [
        "ToolDefinition",
        "ToolCall",
        "ToolCalls",
        "ToolCallID",
        "ToolCallID string",
        "ToolName",
        "ToolArgs",
    ]:
        require(token in provider_go, f"chat provider model missing {token}")
    for token in [
        "tool_calls",
        "makeChatTools",
        "TOOL_CALL",
        "FinishReason",
        "pendingTools",
    ]:
        require(token in openai_go, f"OpenAI provider tool aggregation missing {token}")
    for token in [
        "PetMotionTool",
        "MaxToolCallsPerRun",
        "ToolResultTimeout",
        "SubmitToolResult",
        "appendToolMessages",
        "RUN_STARTED",
        "RUN_FINISHED",
    ]:
        require(token in tools_go + service_go, f"service tool loop missing {token}")
    require('enum":["moveTo","moveBy"]' in tools_go.replace(" ", ""),
            "pet_motion schema must expose moveTo and moveBy only")
    require('"stop"' not in tools_go,
            "pet_motion schema must not expose stop")

    require("/v1/chat/tool-result" in api_go,
            "API server must expose tool result endpoint")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [x] **Step 2: Register contract check in root CMake**

In root `CMakeLists.txt`, after `check_phase_2_4_chat_compact_bubble`, add:

```cmake
        # Phase 2.5 的移动工具调用检查：守住 pet_motion tool、
        # MotionController、ChatController EXECUTING_TOOL 和 sidecar tool-result 循环。
        add_test(
            NAME check_phase_2_5_movement_tool
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_2_5_movement_tool.py
        )
```

- [x] **Step 3: Run contract check**

Run:

```bash
python3 tests/check_phase_2_5_movement_tool.py
ctest --test-dir build -R check_phase_2_5_movement_tool --output-on-failure
```

Expected:

```text
100% tests passed
```

- [x] **Step 4: Commit**

Run:

```bash
git add tests/check_phase_2_5_movement_tool.py CMakeLists.txt
git commit -m "test: 添加移动工具调用契约检查"
```

---

### Task 9: Final Verification and Manual Check

**Files:**
- Read-only, unless failures reveal a bug in prior tasks.

- [ ] **Step 1: Run full focused Qt build**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop MotionControllerSmoke PetRuntimeSmoke ChatControllerSmoke ChatStreamEventParserSmoke SkinManifestLoaderSmoke
```

Expected:

```text
[100%] Built target MilesEdgeworthDesktop
[100%] Built target MotionControllerSmoke
[100%] Built target PetRuntimeSmoke
[100%] Built target ChatControllerSmoke
[100%] Built target ChatStreamEventParserSmoke
[100%] Built target SkinManifestLoaderSmoke
```

- [ ] **Step 2: Run focused CTest suite**

Run:

```bash
ctest --test-dir build -R "motion_controller_smoke|pet_runtime_smoke|chat_controller_smoke|chat_stream_event_parser_smoke|skin_manifest_loader_smoke|check_phase_2_4_phased_animation|check_phase_2_4_2_precut_clips|check_phase_2_5_movement_tool" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 3: Run Go tests**

Run:

```bash
cd apps/agent-core && go test ./...
```

Expected:

```text
ok  	milesedgeworth/agent-core/...
```

- [ ] **Step 4: Manual runtime check**

Run the app:

```bash
cmake --build build --target MilesEdgeworthDesktop
open build/apps/desktop/MilesEdgeworthDesktop.app
```

Manual checks:

- Send a prompt that should call movement, for example: `走到屏幕右下角再告诉我你到了`.
- Confirm Miles speaks first if text arrives before `TOOL_CALL`.
- Confirm movement plays walk/run loop continuously until target arrival.
- Confirm dragging Miles during movement stops movement and the model receives an interrupted result.
- Confirm no `stop` tool action is visible in upstream provider request body.

- [ ] **Step 5: Inspect diff**

Run:

```bash
git status --short
git diff --stat origin/main...HEAD
```

Expected:

```text
```

`git status --short` should be empty. The diff should show only Phase 2.5 implementation, tests, and the plan.

- [ ] **Step 6: Final commit only if needed**

If Step 4 or Step 5 required small fixes, commit them:

```bash
git add <changed-files>
git commit -m "fix: 完善移动工具调用细节"
```

If no files changed, do not create an empty commit.

---

## Self-Review

Spec coverage:

- Tool protocol and `/v1/chat/tool-result`: Task 5 and Task 6.
- `pet_motion` schema with `moveTo` / `moveBy` only: Task 5 and Task 8.
- Qt MotionController target movement, clamp, speed, direction, timer: Task 1.
- Manifest `motion` section: Task 2.
- PetRuntime moving state, loop override, auto movement pause, drag interruption: Task 3 and Task 4.
- ChatController `EXECUTING_TOOL`, tool-only path, timeout, result POST: Task 7.
- Verification and static contract guard: Task 8 and Task 9.

Placeholder scan:

- No placeholder instructions or unspecified implementation steps are used.
- Each code-changing task includes concrete file paths, snippets, commands, and expected outcomes.

Type consistency:

- Go tool types use `ToolDefinition`, `ToolCall`, `ToolCalls`, `ToolCallID`, matching provider/service/API tasks.
- Qt event fields use `toolCallId`, `toolName`, `toolArgs`, matching sidecar JSON and parser tasks.
- Runtime motion methods use `requestMotion`, `stopMotion`, `cancelMotionForDrag`, `motionCompleted`, and `motionInterrupted` consistently.
