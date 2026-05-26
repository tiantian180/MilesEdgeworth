# Mini Chat Polish v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Polish compact chat, expanded chat, and assistant speech bubble according to `docs/superpowers/specs/2026-05-26-mini-chat-polish-v2-design.md`.

**Architecture:** Keep `ChatController` and `ChatTextPacer` unchanged. Put visible-pixel bounds and bubble placement math in small C++ helpers, expose only stable geometry through `DesktopShellController`, and keep visual styling in QML. Reuse `ChatComposer.qml` for compact and expanded modes, with SVG icon assets loaded from Qt resources.

**Tech Stack:** Qt 6.5+ / Qt Quick QML / C++17 / CMake / Ninja / CTest / local SVG resources.

---

## Scope Check

This plan implements one UI polish pass over the existing `feature/chat-compact-bubble` worktree.

Included:

- Compact chat becomes a frameless rounded input bar without a native titlebar or outer window frame.
- Expanded chat becomes a self-drawn rounded window with the same composer style.
- Header, bubble, compact, and composer controls use local Lucide-style SVG assets.
- Pet bubble placement uses visible GIF pixel bounds when available, then falls back to the pet window rectangle.
- Speech bubble width, height, distance, text scrolling, hover controls, and tail anchoring are adjusted.
- C++ smoke tests and Python contract checks guard the geometry and UI contracts.

Excluded:

- No changes to `ChatController`, `ChatTextPacer`, sidecar SSE parsing, provider configuration, persistence, or Phase 2.4 animation sequencing.
- No runtime icon library dependency.
- No unrelated cleanup in settings, pet runtime, or skin manifest code.

## Required Reading

Read these before editing:

- `docs/superpowers/specs/2026-05-26-mini-chat-polish-v2-design.md`
- `docs/superpowers/specs/2026-05-25-mini-chat-ui-redesign.md`
- `docs/v2/设计方案/总体架构设计.md` §4
- `docs/v2/设计方案/AI 聊天动画编排设计.md` §3 and §6
- `apps/desktop/qml/ChatWindow.qml`
- `apps/desktop/qml/ChatComposer.qml`
- `apps/desktop/qml/ChatBubbleWindow.qml`
- `apps/desktop/src/DesktopShellController.{h,cpp}`
- `apps/desktop/src/pet/surface/PetSurfaceWindow.{h,cpp}`
- `apps/desktop/src/chat/ChatBubblePlacement.{h,cpp}`
- `apps/desktop/CMakeLists.txt`

Context7 Qt facts checked for this plan:

- `QMovie::currentImage()` returns the current GIF frame as `QImage`, or a null image if no frame is available.
- `QImage::scanLine()` can be cast to `QRgb*` for 32-bpp images, with `qAlpha()` used to read alpha.
- `QWidget::setMask(QRegion)` already hints the window system to ignore regions outside opaque pixels; the visible-bounds helper should reuse the same alpha threshold concept.
- Transparent windows need frameless flags for consistent desktop translucency.
- `QWindow::startSystemMove()` is the preferred way to drag a custom frameless window when supported.

## File Structure

Create:

- `apps/desktop/qml/MilesIconButton.qml`
  - Small reusable icon-only button for header and bubble controls.
- `apps/desktop/resources/icons/*.svg`
  - Local SVG assets for confirmed UI icons.
- `apps/desktop/src/pet/surface/PetVisibleBounds.h`
  - Pure helper declarations for alpha-bound scanning.
- `apps/desktop/src/pet/surface/PetVisibleBounds.cpp`
  - Testable alpha-bound scanning implementation.
- `apps/desktop/tests/pet_visible_bounds_smoke.cpp`
  - C++ smoke test for visible GIF frame bounds.

Modify:

- `apps/desktop/resources/pet_assets.qrc`
  - Add `/ui-icons` qresource entries.
- `apps/desktop/CMakeLists.txt`
  - Compile `PetVisibleBounds`, add `MilesIconButton.qml`, and register `PetVisibleBoundsSmoke`.
- `apps/desktop/src/DesktopShellController.{h,cpp}`
  - Store current pet visible local bounds and use visible screen geometry for bubble placement.
- `apps/desktop/src/pet/surface/PetSurfaceWindow.{h,cpp}`
  - Compute visible current-frame bounds and push them to `DesktopShellController`.
- `apps/desktop/src/chat/ChatBubblePlacement.{h,cpp}`
  - Prefer centered above/below placement and expose `tailX`.
- `apps/desktop/tests/chat_bubble_placement_smoke.cpp`
  - Replace exact side-margin assertions with centered/tail/clamp assertions.
- `apps/desktop/qml/ChatComposer.qml`
  - Compact dimensions, internal send/stop button centering, icon assets, and drag-friendly non-interactive gaps.
- `apps/desktop/qml/ChatWindow.qml`
  - Frameless transparent window, compact-only rounded bar, expanded self-drawn shell and top bar.
- `apps/desktop/qml/ChatBubbleWindow.qml`
  - New bubble max dimensions, tail anchor, icon controls, and updated path.
- `tests/check_phase_2_4_chat_compact_bubble.py`
  - Update contract checks for icon resources, visible bounds, frameless chat, and tail placement.

Do not modify:

- `apps/desktop/src/chat/ChatController.{h,cpp}`
- `apps/desktop/src/chat/ChatTextPacer.{h,cpp}`
- `apps/agent-core/**`
- `docs/v2/设计方案/**`
- `README.md`, unless implementation changes user-facing build or launch behavior.

---

### Task 0: Preflight Baseline

**Files:**
- Read-only.

- [ ] **Step 1: Confirm worktree and branch**

Run:

```bash
pwd
git status --short --branch
git rev-list --left-right --count origin/main...HEAD
```

Expected:

```text
/Users/tian/.config/superpowers/worktrees/MilesEdgeworth/chat-compact-bubble
## feature/chat-compact-bubble...origin/feature/chat-compact-bubble [ahead N]
0	N
```

The left count must be `0`. The working tree must be clean.

- [ ] **Step 2: Configure and build the current baseline**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target MilesEdgeworthDesktop ChatBubblePlacementSmoke ChatControllerSmoke ChatTextPacerSmoke
```

Expected:

```text
[100%] Built target MilesEdgeworthDesktop
[100%] Built target ChatBubblePlacementSmoke
[100%] Built target ChatControllerSmoke
[100%] Built target ChatTextPacerSmoke
```

- [ ] **Step 3: Run focused baseline tests**

Run:

```bash
ctest --test-dir build -R "chat_bubble_placement_smoke|chat_controller_smoke|chat_text_pacer_smoke|check_phase_2_4_chat_compact_bubble" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 4: No commit**

This task changes nothing.

---

### Task 1: Icon Assets and Reusable Icon Button

**Files:**
- Create: `apps/desktop/qml/MilesIconButton.qml`
- Create: `apps/desktop/resources/icons/menu.svg`
- Create: `apps/desktop/resources/icons/settings.svg`
- Create: `apps/desktop/resources/icons/refresh-ccw.svg`
- Create: `apps/desktop/resources/icons/maximize-2.svg`
- Create: `apps/desktop/resources/icons/minimize-2.svg`
- Create: `apps/desktop/resources/icons/x.svg`
- Create: `apps/desktop/resources/icons/arrow-up-white.svg`
- Create: `apps/desktop/resources/icons/arrow-up-muted.svg`
- Create: `apps/desktop/resources/icons/square-stop.svg`
- Modify: `apps/desktop/resources/pet_assets.qrc`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `tests/check_phase_2_4_chat_compact_bubble.py`

- [ ] **Step 1: Add a failing contract check for icon resources**

In `tests/check_phase_2_4_chat_compact_bubble.py`, read `apps/desktop/resources/pet_assets.qrc` and add checks for the icon resources and QML component. Use this exact block after the current `desktop_cmake = read(...)` line:

```python
    pet_assets_qrc = read("apps/desktop/resources/pet_assets.qrc")
```

Add these requirements before the existing ChatWindow checks:

```python
    for token in [
        'qml/MilesIconButton.qml',
        '<qresource prefix="/ui-icons">',
        'alias="settings.svg"',
        'alias="refresh-ccw.svg"',
        'alias="maximize-2.svg"',
        'alias="minimize-2.svg"',
        'alias="x.svg"',
        'alias="menu.svg"',
        'alias="arrow-up-white.svg"',
        'alias="arrow-up-muted.svg"',
        'alias="square-stop.svg"',
    ]:
        require(token in desktop_cmake + pet_assets_qrc,
                f"icon resource contract missing {token}")
```

- [ ] **Step 2: Run the contract check and verify it fails**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
```

Expected failure:

```text
AssertionError: icon resource contract missing qml/MilesIconButton.qml
```

- [ ] **Step 3: Create SVG resources**

Create the SVG files under `apps/desktop/resources/icons/`. Use the exact `fill="none"` stroke style for line icons so they do not render as black blobs.

`apps/desktop/resources/icons/menu.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#746d65" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M4 5h16"/>
  <path d="M4 12h16"/>
  <path d="M4 19h16"/>
</svg>
```

`apps/desktop/resources/icons/settings.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#746d65" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M9.671 4.136a2.34 2.34 0 0 1 4.659 0 2.34 2.34 0 0 0 3.319 1.915 2.34 2.34 0 0 1 2.33 4.033 2.34 2.34 0 0 0 0 3.831 2.34 2.34 0 0 1-2.33 4.033 2.34 2.34 0 0 0-3.319 1.915 2.34 2.34 0 0 1-4.659 0 2.34 2.34 0 0 0-3.32-1.915 2.34 2.34 0 0 1-2.33-4.033 2.34 2.34 0 0 0 0-3.831A2.34 2.34 0 0 1 6.35 6.051a2.34 2.34 0 0 0 3.319-1.915"/>
  <circle cx="12" cy="12" r="3"/>
</svg>
```

`apps/desktop/resources/icons/refresh-ccw.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#746d65" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M21 12a9 9 0 0 0-9-9 9.75 9.75 0 0 0-6.74 2.74L3 8"/>
  <path d="M3 3v5h5"/>
  <path d="M3 12a9 9 0 0 0 9 9 9.75 9.75 0 0 0 6.74-2.74L21 16"/>
  <path d="M16 16h5v5"/>
</svg>
```

`apps/desktop/resources/icons/maximize-2.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#746d65" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M15 3h6v6"/>
  <path d="m21 3-7 7"/>
  <path d="m3 21 7-7"/>
  <path d="M9 21H3v-6"/>
</svg>
```

`apps/desktop/resources/icons/minimize-2.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#746d65" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="m14 10 7-7"/>
  <path d="M20 10h-6V4"/>
  <path d="m3 21 7-7"/>
  <path d="M4 14h6v6"/>
</svg>
```

`apps/desktop/resources/icons/x.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#746d65" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M18 6 6 18"/>
  <path d="m6 6 12 12"/>
</svg>
```

`apps/desktop/resources/icons/arrow-up-white.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#fffdf8" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round">
  <path d="m5 12 7-7 7 7"/>
  <path d="M12 19V5"/>
</svg>
```

`apps/desktop/resources/icons/arrow-up-muted.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#7d746d" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round">
  <path d="m5 12 7-7 7 7"/>
  <path d="M12 19V5"/>
</svg>
```

`apps/desktop/resources/icons/square-stop.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
  <rect x="7" y="7" width="10" height="10" rx="2" fill="#5f554b"/>
</svg>
```

- [ ] **Step 4: Add the QML icon button**

Create `apps/desktop/qml/MilesIconButton.qml`:

```qml
import QtQuick
import QtQuick.Controls

ToolButton {
    id: control

    property url iconSource
    property string tooltipText: ""
    property int iconSize: 20
    property color hoverFill: "#eee7da"
    property color pressedFill: "#e2d8cc"
    property bool showHoverFill: true

    width: 34
    height: 34
    padding: 0

    ToolTip.visible: hovered && tooltipText.length > 0
    ToolTip.text: tooltipText

    contentItem: Image {
        source: control.iconSource
        sourceSize.width: control.iconSize
        sourceSize.height: control.iconSize
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        opacity: control.enabled ? 1.0 : 0.42
        smooth: true
    }

    background: Rectangle {
        radius: 10
        color: !control.showHoverFill
                ? "transparent"
                : (control.down ? control.pressedFill : (control.hovered ? control.hoverFill : "transparent"))
    }
}
```

- [ ] **Step 5: Register resources and QML file**

In `apps/desktop/resources/pet_assets.qrc`, add this qresource before `</RCC>`:

```xml
  <qresource prefix="/ui-icons">
    <file alias="menu.svg">icons/menu.svg</file>
    <file alias="settings.svg">icons/settings.svg</file>
    <file alias="refresh-ccw.svg">icons/refresh-ccw.svg</file>
    <file alias="maximize-2.svg">icons/maximize-2.svg</file>
    <file alias="minimize-2.svg">icons/minimize-2.svg</file>
    <file alias="x.svg">icons/x.svg</file>
    <file alias="arrow-up-white.svg">icons/arrow-up-white.svg</file>
    <file alias="arrow-up-muted.svg">icons/arrow-up-muted.svg</file>
    <file alias="square-stop.svg">icons/square-stop.svg</file>
  </qresource>
```

In `apps/desktop/CMakeLists.txt`, add `qml/MilesIconButton.qml` to the `qt_add_qml_module(... QML_FILES ...)` list next to `qml/ChatComposer.qml`.

- [ ] **Step 6: Verify the contract passes**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
```

Expected:

```text
phase 2.4 compact chat bubble contract ok
```

- [ ] **Step 7: Commit**

Run:

```bash
git add apps/desktop/qml/MilesIconButton.qml apps/desktop/resources/icons apps/desktop/resources/pet_assets.qrc apps/desktop/CMakeLists.txt tests/check_phase_2_4_chat_compact_bubble.py
git commit -m "feat: 添加聊天窗线性图标资源"
```

---

### Task 2: Pet Visible Pixel Bounds

**Files:**
- Create: `apps/desktop/src/pet/surface/PetVisibleBounds.h`
- Create: `apps/desktop/src/pet/surface/PetVisibleBounds.cpp`
- Create: `apps/desktop/tests/pet_visible_bounds_smoke.cpp`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/src/DesktopShellController.h`
- Modify: `apps/desktop/src/DesktopShellController.cpp`
- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.h`
- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`
- Modify: `tests/check_phase_2_4_chat_compact_bubble.py`

- [ ] **Step 1: Write the failing visible-bounds smoke test**

Create `apps/desktop/tests/pet_visible_bounds_smoke.cpp`:

```cpp
#include "pet/surface/PetVisibleBounds.h"

#include <QColor>
#include <QImage>
#include <QRect>

#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    {
        QImage image(6, 5, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        image.setPixelColor(1, 2, QColor(10, 20, 30, 255));
        image.setPixelColor(2, 2, QColor(10, 20, 30, 200));
        image.setPixelColor(3, 3, QColor(10, 20, 30, 120));

        const QRect bounds = visibleBoundsFromImage(image, 8);
        require(bounds == QRect(1, 2, 3, 2), "alpha bounds should cover opaque pixels only");
    }

    {
        QImage image(4, 4, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        image.setPixelColor(2, 2, QColor(10, 20, 30, 7));

        const QRect bounds = visibleBoundsFromImage(image, 8);
        require(!bounds.isValid(), "pixels below threshold should not count as visible");
    }

    {
        QImage image(3, 2, QImage::Format_RGB32);
        image.fill(QColor(40, 50, 60));

        const QRect bounds = visibleBoundsFromImage(image, 8);
        require(bounds == QRect(0, 0, 3, 2), "opaque RGB images should be fully visible");
    }

    {
        const QRect bounds = visibleBoundsFromImage(QImage(), 8);
        require(!bounds.isValid(), "null image should produce invalid bounds");
    }

    return 0;
}
```

- [ ] **Step 2: Register the test before implementation**

In `apps/desktop/CMakeLists.txt`, add this test target inside `if(BUILD_TESTING)` after `ChatBubblePlacementSmoke`:

```cmake
    add_executable(PetVisibleBoundsSmoke
        tests/pet_visible_bounds_smoke.cpp
        src/pet/surface/PetVisibleBounds.cpp
        src/pet/surface/PetVisibleBounds.h
    )

    target_include_directories(PetVisibleBoundsSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(PetVisibleBoundsSmoke
        PRIVATE
            Qt6::Core
            Qt6::Gui
    )

    add_test(NAME pet_visible_bounds_smoke COMMAND PetVisibleBoundsSmoke)
```

Also add these two files to `DESKTOP_SOURCES`:

```cmake
    src/pet/surface/PetVisibleBounds.cpp
    src/pet/surface/PetVisibleBounds.h
```

- [ ] **Step 3: Run the test and verify it fails to build**

Run:

```bash
cmake --build build --target PetVisibleBoundsSmoke
```

Expected failure:

```text
fatal error: 'pet/surface/PetVisibleBounds.h' file not found
```

- [ ] **Step 4: Implement the visible-bounds helper**

Create `apps/desktop/src/pet/surface/PetVisibleBounds.h`:

```cpp
#pragma once

#include <QImage>
#include <QRect>

QRect visibleBoundsFromImage(const QImage &image, int alphaThreshold);
```

Create `apps/desktop/src/pet/surface/PetVisibleBounds.cpp`:

```cpp
#include "pet/surface/PetVisibleBounds.h"

#include <QColor>
#include <QtGlobal>

QRect visibleBoundsFromImage(const QImage &sourceImage, int alphaThreshold)
{
    if (sourceImage.isNull()) {
        return {};
    }

    const QImage image = sourceImage.format() == QImage::Format_ARGB32
        ? sourceImage
        : sourceImage.convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        return {};
    }

    const int threshold = qBound(0, alphaThreshold, 255);
    int minX = image.width();
    int minY = image.height();
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < image.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(row[x]) <= threshold) {
                continue;
            }

            minX = qMin(minX, x);
            minY = qMin(minY, y);
            maxX = qMax(maxX, x);
            maxY = qMax(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY) {
        return {};
    }

    return QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}
```

- [ ] **Step 5: Verify the helper passes**

Run:

```bash
cmake --build build --target PetVisibleBoundsSmoke
ctest --test-dir build -R pet_visible_bounds_smoke --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 6: Add visible-bounds storage to DesktopShellController**

In `apps/desktop/src/DesktopShellController.h`, add:

```cpp
#include <QRect>
```

Add a public method near `setPetWindow(QWindow *window);`:

```cpp
    void setPetVisibleLocalBounds(const QRect &bounds);
```

Add a private method near `petScreenAvailableGeometry() const;`:

```cpp
    QRect petVisibleScreenGeometry() const;
```

Add a private member:

```cpp
    QRect m_petVisibleLocalBounds;
```

In `apps/desktop/src/DesktopShellController.cpp`, implement:

```cpp
void DesktopShellController::setPetVisibleLocalBounds(const QRect &bounds)
{
    const QRect normalizedBounds = bounds.isValid() ? bounds : QRect();
    if (m_petVisibleLocalBounds == normalizedBounds) {
        return;
    }

    m_petVisibleLocalBounds = normalizedBounds;
    emit petWindowGeometryChanged();
}
```

Add:

```cpp
QRect DesktopShellController::petVisibleScreenGeometry() const
{
    if (m_petWindow == nullptr || !m_petVisibleLocalBounds.isValid()) {
        return QRect(petWindowX(), petWindowY(), petWindowWidth(), petWindowHeight());
    }

    return QRect(
        QPoint(m_petWindow->x() + m_petVisibleLocalBounds.x(),
               m_petWindow->y() + m_petVisibleLocalBounds.y()),
        m_petVisibleLocalBounds.size()
    );
}
```

In `setPetWindow(QWindow *window)`, clear stale bounds when switching windows:

```cpp
    m_petVisibleLocalBounds = QRect();
```

Do not change `placeChatBubble(...)` in this task; Task 3 switches it to visible geometry.

- [ ] **Step 7: Push current-frame visible bounds from PetSurfaceWindow**

In `apps/desktop/src/pet/surface/PetSurfaceWindow.h`, add private methods:

```cpp
    QRect visibleLocalBoundsFromCurrentFrame() const;
    void syncVisibleBoundsToShell();
```

In `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`, include the helper:

```cpp
#include "pet/surface/PetVisibleBounds.h"
```

Add the implementation:

```cpp
QRect PetSurfaceWindow::visibleLocalBoundsFromCurrentFrame() const
{
    if (m_movie == nullptr || m_petLabel == nullptr || m_petLabel->size().isEmpty()) {
        return {};
    }

    QImage currentFrame = m_movie->currentImage();
    if (currentFrame.isNull()) {
        const QPixmap currentPixmap = m_movie->currentPixmap();
        if (!currentPixmap.isNull()) {
            currentFrame = currentPixmap.toImage();
        }
    }
    if (currentFrame.isNull()) {
        return {};
    }

    const QImage scaledFrame = currentFrame
        .scaled(m_petLabel->size(), Qt::IgnoreAspectRatio, Qt::FastTransformation)
        .convertToFormat(QImage::Format_ARGB32);
    QRect visibleBounds = visibleBoundsFromImage(scaledFrame, kAlphaThreshold);
    if (!visibleBounds.isValid()) {
        return {};
    }

    visibleBounds.translate(m_petLabel->pos());
    return visibleBounds;
}

void PetSurfaceWindow::syncVisibleBoundsToShell()
{
    if (m_shellController == nullptr) {
        return;
    }

    m_shellController->setPetVisibleLocalBounds(visibleLocalBoundsFromCurrentFrame());
}
```

Call `syncVisibleBoundsToShell();` after each `applyCurrentFrameMask();` call in:

- `resizeEvent(QResizeEvent *event)`
- `syncSizeFromRuntime()`
- `restartMovieFromRuntime()`
- `handleMovieFrameChanged(int frame)`

When `restartMovieFromRuntime()` clears the movie because the path is empty, call:

```cpp
        m_shellController->setPetVisibleLocalBounds(QRect());
```

- [ ] **Step 8: Update contract checks**

In `tests/check_phase_2_4_chat_compact_bubble.py`, read:

```python
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    visible_bounds_h = read("apps/desktop/src/pet/surface/PetVisibleBounds.h")
    visible_bounds_cpp = read("apps/desktop/src/pet/surface/PetVisibleBounds.cpp")
    visible_bounds_smoke = read("apps/desktop/tests/pet_visible_bounds_smoke.cpp")
```

Add:

```python
    for token in [
        "PetVisibleBounds.cpp",
        "PetVisibleBoundsSmoke",
        "pet_visible_bounds_smoke",
        "setPetVisibleLocalBounds",
        "petVisibleScreenGeometry",
    ]:
        require(token in desktop_cmake + shell_h + shell_cpp,
                f"visible pet bounds bridge missing {token}")

    for token in [
        "visibleBoundsFromImage",
        "QImage::Format_ARGB32",
        "constScanLine",
        "qAlpha",
    ]:
        require(token in visible_bounds_h + visible_bounds_cpp,
                f"PetVisibleBounds missing {token}")

    for token in [
        "visibleLocalBoundsFromCurrentFrame",
        "syncVisibleBoundsToShell",
        "m_movie->currentImage()",
        "m_petLabel->size()",
        "m_shellController->setPetVisibleLocalBounds",
    ]:
        require(token in surface_h + surface_cpp,
                f"PetSurfaceWindow visible bounds sync missing {token}")

    require("alpha bounds should cover opaque pixels only" in visible_bounds_smoke,
            "visible bounds smoke must cover alpha scanning")
```

- [ ] **Step 9: Run focused tests**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop PetVisibleBoundsSmoke
ctest --test-dir build -R "pet_visible_bounds_smoke|check_phase_2_4_chat_compact_bubble" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 10: Commit**

Run:

```bash
git add apps/desktop/src/pet/surface/PetVisibleBounds.* apps/desktop/tests/pet_visible_bounds_smoke.cpp apps/desktop/src/DesktopShellController.* apps/desktop/src/pet/surface/PetSurfaceWindow.* apps/desktop/CMakeLists.txt tests/check_phase_2_4_chat_compact_bubble.py
git commit -m "feat: 基于桌宠可见像素定位气泡"
```

---

### Task 3: Centered Bubble Placement with Tail Anchor

**Files:**
- Modify: `apps/desktop/src/chat/ChatBubblePlacement.h`
- Modify: `apps/desktop/src/chat/ChatBubblePlacement.cpp`
- Modify: `apps/desktop/tests/chat_bubble_placement_smoke.cpp`
- Modify: `apps/desktop/src/DesktopShellController.cpp`
- Modify: `tests/check_phase_2_4_chat_compact_bubble.py`

- [ ] **Step 1: Replace the placement smoke test with the new contract**

Replace `apps/desktop/tests/chat_bubble_placement_smoke.cpp` with:

```cpp
#include "chat/ChatBubblePlacement.h"

#include <QRect>
#include <QSize>
#include <QString>

#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireInsideAvailable(const ChatBubblePlacementResult &result, const QRect &available, const QSize &bubbleSize, int margin)
{
    require(result.topLeft.x() >= available.left() + margin, "bubble should not cross left available margin");
    require(result.topLeft.y() >= available.top() + margin, "bubble should not cross top available margin");
    require(result.topLeft.x() + bubbleSize.width() <= available.right() - margin + 1,
            "bubble should not cross right available margin");
    require(result.topLeft.y() + bubbleSize.height() <= available.bottom() - margin + 1,
            "bubble should not cross bottom available margin");
}
} // namespace

int main()
{
    const QRect screen(0, 0, 1000, 800);
    const QSize bubbleSize(320, 156);
    const int margin = 12;

    {
        const QRect pet(24, 32, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("topLeft"), "top-left pet should show bubble below with left tail");
        require(result.topLeft.y() == pet.bottom() + 1 + margin, "top-left pet should keep vertical gap below pet");
        require(result.tailX >= 42 && result.tailX <= bubbleSize.width() - 42, "tailX should stay inside safe tail range");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect pet(880, 32, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("topRight"), "top-right pet should show bubble below with right tail");
        require(result.topLeft.y() == pet.bottom() + 1 + margin, "top-right pet should keep vertical gap below pet");
        require(result.tailX >= 42 && result.tailX <= bubbleSize.width() - 42, "tailX should stay inside safe tail range");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect pet(24, 636, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("bottomLeft"), "bottom-left pet should show bubble above with left tail");
        require(result.topLeft.y() + bubbleSize.height() == pet.top() - margin, "bottom-left pet should keep vertical gap above pet");
        require(result.tailX >= 42 && result.tailX <= bubbleSize.width() - 42, "tailX should stay inside safe tail range");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect pet(880, 636, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("bottomRight"), "bottom-right pet should show bubble above with right tail");
        require(result.topLeft.y() + bubbleSize.height() == pet.top() - margin, "bottom-right pet should keep vertical gap above pet");
        require(result.tailX >= 42 && result.tailX <= bubbleSize.width() - 42, "tailX should stay inside safe tail range");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect pet(430, 520, 100, 140);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer.startsWith(QStringLiteral("bottom")), "pet with room above should place bubble above");
        require(result.topLeft.x() == pet.center().x() - bubbleSize.width() / 2,
                "bubble should center horizontally over pet when room exists");
        require(result.tailX == bubbleSize.width() / 2, "centered bubble should use centered tail");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect tinyScreen(0, 0, 240, 160);
        const QRect pet(90, 70, 80, 80);
        const QSize largeBubble(300, 220);
        const ChatBubblePlacementResult result = placeChatBubble(pet, tinyScreen, largeBubble, margin);
        require(result.topLeft.x() == margin, "oversized bubble should clamp to left margin");
        require(result.topLeft.y() == margin, "oversized bubble should clamp to top margin");
        require(result.tailX >= 42 && result.tailX <= largeBubble.width() - 42, "oversized tailX should still be safe");
    }

    {
        const QRect shiftedScreen(-500, -300, 500, 400);
        const QRect pet(-450, -222, 80, 240);
        const ChatBubblePlacementResult result = placeChatBubble(pet, shiftedScreen, bubbleSize, margin);
        requireInsideAvailable(result, shiftedScreen, bubbleSize, margin);
    }

    return 0;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
cmake --build build --target ChatBubblePlacementSmoke
ctest --test-dir build -R chat_bubble_placement_smoke --output-on-failure
```

Expected failure:

```text
centered bubble should use centered tail
```

- [ ] **Step 3: Add `tailX` to the placement result**

In `apps/desktop/src/chat/ChatBubblePlacement.h`, change the struct to:

```cpp
struct ChatBubblePlacementResult
{
    QPoint topLeft;
    QString pointer;
    int tailX = 0;
};
```

- [ ] **Step 4: Replace the placement algorithm**

Replace the body of `placeChatBubble(...)` in `apps/desktop/src/chat/ChatBubblePlacement.cpp` with:

```cpp
{
    const QRect available = availableGeometry.isValid()
        ? availableGeometry
        : QRect(0, 0, safeDimension(bubbleSize.width()), safeDimension(bubbleSize.height()));
    const QRect pet = petGeometry.isValid()
        ? petGeometry
        : QRect(available.center().x(), available.center().y(), 1, 1);
    const int safeMargin = qMax(0, margin);
    const int bubbleWidth = safeDimension(bubbleSize.width());
    const int bubbleHeight = safeDimension(bubbleSize.height());
    const int tailInset = qMin(qMax(42, safeMargin * 3), qMax(42, bubbleWidth / 2));

    const QPoint petCenter = pet.center();
    const int minX = available.left() + safeMargin;
    const int minY = available.top() + safeMargin;
    const int maxX = available.right() - bubbleWidth - safeMargin + 1;
    const int maxY = available.bottom() - bubbleHeight - safeMargin + 1;

    const int preferredX = petCenter.x() - bubbleWidth / 2;
    const int aboveY = pet.top() - bubbleHeight - safeMargin;
    const int belowY = pet.bottom() + 1 + safeMargin;
    const bool canPlaceAbove = aboveY >= minY;
    const bool canPlaceBelow = belowY <= maxY;
    const bool placeAbove = canPlaceAbove || !canPlaceBelow;
    const int preferredY = placeAbove ? aboveY : belowY;

    ChatBubblePlacementResult result;
    result.topLeft = QPoint(
        clampCoordinate(preferredX, minX, maxX),
        clampCoordinate(preferredY, minY, maxY)
    );

    result.tailX = clampCoordinate(petCenter.x() - result.topLeft.x(), tailInset, bubbleWidth - tailInset);
    const bool tailOnLeft = result.tailX <= bubbleWidth / 2;
    if (placeAbove) {
        result.pointer = tailOnLeft ? QStringLiteral("bottomLeft") : QStringLiteral("bottomRight");
    } else {
        result.pointer = tailOnLeft ? QStringLiteral("topLeft") : QStringLiteral("topRight");
    }

    return result;
}
```

- [ ] **Step 5: Use visible geometry and expose tailX to QML**

In `apps/desktop/src/DesktopShellController.cpp`, change `placeChatBubble(...)` to use visible geometry:

```cpp
    const QRect petGeometry = petVisibleScreenGeometry();
```

Add `tailX` to the result map:

```cpp
    result.insert(QStringLiteral("tailX"), placement.tailX);
```

- [ ] **Step 6: Update the contract check**

In `tests/check_phase_2_4_chat_compact_bubble.py`, replace the old placement tokens that require exact side margins with:

```python
    for token in [
        "tailX",
        "petCenter.x() - bubbleWidth / 2",
        "canPlaceAbove",
        "result.tailX = clampCoordinate",
        "QStringLiteral(\"bottomLeft\")",
        "QStringLiteral(\"bottomRight\")",
        "QStringLiteral(\"topLeft\")",
        "QStringLiteral(\"topRight\")",
    ]:
        require(token in placement_cpp, f"ChatBubblePlacement.cpp missing {token}")

    for token in [
        "centered bubble should use centered tail",
        "pet with room above should place bubble above",
        "tailX should stay inside safe tail range",
    ]:
        require(token in placement_smoke, f"chat_bubble_placement_smoke.cpp missing {token}")

    require("result.insert(QStringLiteral(\"tailX\"), placement.tailX);" in shell_cpp,
            "DesktopShellController.placeChatBubble must expose tailX")
```

- [ ] **Step 7: Run focused tests**

Run:

```bash
cmake --build build --target ChatBubblePlacementSmoke MilesEdgeworthDesktop
ctest --test-dir build -R "chat_bubble_placement_smoke|check_phase_2_4_chat_compact_bubble" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 8: Commit**

Run:

```bash
git add apps/desktop/src/chat/ChatBubblePlacement.* apps/desktop/tests/chat_bubble_placement_smoke.cpp apps/desktop/src/DesktopShellController.cpp tests/check_phase_2_4_chat_compact_bubble.py
git commit -m "fix: 让聊天气泡贴近桌宠可见中心"
```

---

### Task 4: Compact Composer Visual and Drag Behavior

**Files:**
- Modify: `apps/desktop/qml/ChatComposer.qml`
- Modify: `apps/desktop/qml/ChatWindow.qml`
- Modify: `tests/check_phase_2_4_chat_compact_bubble.py`

- [ ] **Step 1: Add failing contract checks for compact dimensions and icon sources**

In `tests/check_phase_2_4_chat_compact_bubble.py`, add these ChatComposer tokens:

```python
    composer_qml = read("apps/desktop/qml/ChatComposer.qml")

    for token in [
        "property url expandIconSource: \"qrc:/ui-icons/maximize-2.svg\"",
        "property url sendIconSource",
        "property url stopIconSource: \"qrc:/ui-icons/square-stop.svg\"",
        "readonly property int compactBarWidth: 380",
        "readonly property int inputMaxHeight: compactMode ? 140 : 156",
        "MilesIconButton",
        "arrow-up-white.svg",
        "arrow-up-muted.svg",
    ]:
        require(token in composer_qml, f"ChatComposer.qml missing compact polish token {token}")
```

Add these ChatWindow tokens:

```python
    for token in [
        "property int compactWidth: 380",
        "height = compactComposer.implicitHeight",
        "maximumHeight = compactComposer.implicitHeight",
        "compactDragArea",
        "z: 0",
        "z: 1",
        "startSystemMove()",
    ]:
        require(token in chat_qml, f"ChatWindow.qml missing compact shell token {token}")
```

- [ ] **Step 2: Run the contract check and verify it fails**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
```

Expected failure:

```text
AssertionError: ChatComposer.qml missing compact polish token property url expandIconSource: "qrc:/ui-icons/maximize-2.svg"
```

- [ ] **Step 3: Update ChatComposer properties**

In `apps/desktop/qml/ChatComposer.qml`, add these properties near the existing properties:

```qml
    property url expandIconSource: "qrc:/ui-icons/maximize-2.svg"
    property url closeIconSource: "qrc:/ui-icons/x.svg"
    property url sendIconSource: canSubmit && !App.ChatController.sending
            ? "qrc:/ui-icons/arrow-up-white.svg"
            : "qrc:/ui-icons/arrow-up-muted.svg"
    property url stopIconSource: "qrc:/ui-icons/square-stop.svg"
    readonly property int compactBarWidth: 380
```

Change sizing properties to:

```qml
    readonly property int inputMinHeight: compactMode ? 42 : 52
    readonly property int inputMaxHeight: compactMode ? 140 : 156
    readonly property int inputTargetHeight: Math.max(inputMinHeight,
            Math.min(inputMaxHeight, Math.ceil(input.contentHeight + 18)))
    readonly property int outerVerticalPadding: compactMode ? 7 : 14
    readonly property int buttonSize: compactMode ? 34 : 38

    implicitWidth: compactMode ? compactBarWidth : 520
    implicitHeight: inputTargetHeight + outerVerticalPadding * 2
```

- [ ] **Step 4: Replace compact expand and close buttons**

In `ChatComposer.qml`, replace the compact expand `ToolButton` with:

```qml
        MilesIconButton {
            id: expandButton

            visible: root.compactMode
            iconSource: root.expandIconSource
            iconSize: 20
            tooltipText: "展开聊天"
            showHoverFill: false
            Layout.preferredWidth: visible ? 36 : 0
            Layout.preferredHeight: root.inputMinHeight
            onClicked: root.expandRequested()
        }
```

Replace the compact close `ToolButton` with:

```qml
        MilesIconButton {
            id: closeButton

            visible: root.compactMode
            iconSource: root.closeIconSource
            iconSize: 20
            tooltipText: "隐藏聊天"
            showHoverFill: false
            Layout.preferredWidth: visible ? 36 : 0
            Layout.preferredHeight: root.inputMinHeight
            onClicked: root.closeRequested()
        }
```

- [ ] **Step 5: Center the send/stop icon**

In `ChatComposer.qml`, replace the `sendButton` text/content with an `Image`:

```qml
                text: ""
                padding: 0
                contentItem: Image {
                    source: App.ChatController.sending ? root.stopIconSource : root.sendIconSource
                    sourceSize.width: App.ChatController.sending ? 18 : 22
                    sourceSize.height: App.ChatController.sending ? 18 : 22
                    fillMode: Image.PreserveAspectFit
                    horizontalAlignment: Image.AlignHCenter
                    verticalAlignment: Image.AlignVCenter
                    smooth: true
                }
```

Keep the existing `onClicked: root.submitOrCancel()`. Change the button background radius to a fully centered pill:

```qml
                    radius: root.buttonSize / 2
```

- [ ] **Step 6: Remove the compact outer frame from ChatWindow**

In `apps/desktop/qml/ChatWindow.qml`, change compact defaults:

```qml
    property int compactWidth: 380
    property int compactMinHeight: 56
```

In `showCompact()`, set:

```qml
        minimumWidth = compactWidth
        minimumHeight = compactMinHeight
        maximumHeight = compactComposer.implicitHeight
        width = compactWidth
        height = compactComposer.implicitHeight
```

In `compactComposer.onImplicitHeightChanged`, set:

```qml
                        chatWindow.maximumHeight = implicitHeight
                        chatWindow.height = implicitHeight
```

Remove the `anchors.margins: chatWindow.compactMode ? 8 : 0` margin from `compactComposer`; use `anchors.fill: parent`.

Change `compactDragShell` compact styling to one clean bar:

```qml
            radius: chatWindow.compactMode ? 18 : 0
            color: chatWindow.compactMode ? "#fffdf8" : "transparent"
            border.color: chatWindow.compactMode ? "#d8cec1" : "transparent"
            border.width: chatWindow.compactMode ? 1 : 0
```

- [ ] **Step 7: Add drag area behind compact controls**

Inside `compactDragShell`, before `ChatComposer`, add:

```qml
            MouseArea {
                id: compactDragArea

                anchors.fill: parent
                z: 0
                enabled: chatWindow.compactMode
                acceptedButtons: Qt.LeftButton
                onPressed: chatWindow.startSystemMove()
            }
```

Set the `ChatComposer` z:

```qml
                z: 1
```

Remove the old top-only 8px `MouseArea`; the new background drag area covers all non-interactive blank space while controls remain above it.

- [ ] **Step 8: Run focused contract and QML build**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
cmake --build build --target MilesEdgeworthDesktop
```

Expected:

```text
phase 2.4 compact chat bubble contract ok
[100%] Built target MilesEdgeworthDesktop
```

- [ ] **Step 9: Commit**

Run:

```bash
git add apps/desktop/qml/ChatComposer.qml apps/desktop/qml/ChatWindow.qml tests/check_phase_2_4_chat_compact_bubble.py
git commit -m "fix: 打磨迷你聊天输入条"
```

---

### Task 5: Frameless Expanded Chat Window

**Files:**
- Modify: `apps/desktop/qml/ChatWindow.qml`
- Modify: `tests/check_phase_2_4_chat_compact_bubble.py`

- [ ] **Step 1: Add failing contract checks for expanded shell**

In `tests/check_phase_2_4_chat_compact_bubble.py`, add:

```python
    for token in [
        "flags: Qt.Window | Qt.FramelessWindowHint",
        "color: \"transparent\"",
        "id: expandedShell",
        "id: expandedTitleBar",
        "qrc:/ui-icons/menu.svg",
        "qrc:/ui-icons/settings.svg",
        "qrc:/ui-icons/refresh-ccw.svg",
        "qrc:/ui-icons/minimize-2.svg",
        "expandedTitleDragArea",
        "showExpanded()",
        "showCompact()",
    ]:
        require(token in chat_qml, f"ChatWindow.qml missing expanded polish token {token}")
```

- [ ] **Step 2: Run the contract check and verify it fails**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
```

Expected failure:

```text
AssertionError: ChatWindow.qml missing expanded polish token flags: Qt.Window | Qt.FramelessWindowHint
```

- [ ] **Step 3: Make ChatWindow transparent and frameless**

In `apps/desktop/qml/ChatWindow.qml`, change:

```qml
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
```

Keep `title: "Miles Chat"` for accessibility/window identification.

- [ ] **Step 4: Add expanded shell container**

Wrap the expanded content in:

```qml
        Rectangle {
            id: expandedShell

            visible: !chatWindow.compactMode
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 22
            color: "#fffaf2"
            border.color: "#d8cec1"
            border.width: 1
            clip: true
        }
```

Move the existing expanded header, conversation panel, transcript, and composer into this shell. The compact `compactDragShell` remains a sibling so compact mode only shows the input bar.

- [ ] **Step 5: Replace the expanded top bar**

Inside `expandedShell`, create this top bar:

```qml
            Rectangle {
                id: expandedTitleBar

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 54
                color: "#fffaf2"

                MouseArea {
                    id: expandedTitleDragArea

                    anchors.fill: parent
                    z: 0
                    acceptedButtons: Qt.LeftButton
                    onPressed: chatWindow.startSystemMove()
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    z: 1
                    spacing: 10

                    MilesIconButton {
                        iconSource: "qrc:/ui-icons/menu.svg"
                        tooltipText: "会话"
                        onClicked: chatWindow.conversationPanelOpen = !chatWindow.conversationPanelOpen
                    }

                    Label {
                        text: "Miles"
                        color: "#2d2925"
                        font.pixelSize: 19
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: App.ChatController.sidecarReady ? "#4d985d" : "#b65a45"
                    }

                    Label {
                        text: App.ChatController.statusText
                        color: App.ChatController.sidecarReady ? "#557c59" : "#8a4b38"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        Layout.maximumWidth: 128
                    }

                    Item { Layout.fillWidth: true }

                    MilesIconButton {
                        iconSource: "qrc:/ui-icons/settings.svg"
                        tooltipText: "设置"
                        onClicked: App.SettingsController.openWindow()
                    }

                    MilesIconButton {
                        iconSource: "qrc:/ui-icons/refresh-ccw.svg"
                        tooltipText: "重连"
                        enabled: !App.ChatController.sending
                        onClicked: {
                            App.ChatController.startSidecar()
                            App.ChatController.checkHealth()
                        }
                    }

                    MilesIconButton {
                        iconSource: "qrc:/ui-icons/minimize-2.svg"
                        tooltipText: "收起"
                        onClicked: chatWindow.showCompact()
                    }

                    MilesIconButton {
                        iconSource: "qrc:/ui-icons/x.svg"
                        tooltipText: "隐藏聊天"
                        onClicked: chatWindow.hideChatUi()
                    }
                }
            }
```

Remove the old text buttons `设置`, `重连`, and `收起` from the expanded header.

- [ ] **Step 6: Restyle transcript and composer area**

Inside `expandedShell`, place the transcript area below `expandedTitleBar` and above the composer. Use these visual values:

```qml
color: "#fffaf2"
border.color: "transparent"
radius: 0
```

For assistant message bubbles:

```qml
color: error ? "#f6d6cc" : (isUser ? "#dcecff" : "#fffdf8")
border.color: error ? "#b65a45" : (isUser ? "transparent" : "#ded4c8")
radius: 13
```

For the bottom composer area, remove any extra wrapper border in expanded mode:

```qml
color: "transparent"
border.color: "transparent"
border.width: 0
```

- [ ] **Step 7: Run build and focused contract**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
cmake --build build --target MilesEdgeworthDesktop
```

Expected:

```text
phase 2.4 compact chat bubble contract ok
[100%] Built target MilesEdgeworthDesktop
```

- [ ] **Step 8: Commit**

Run:

```bash
git add apps/desktop/qml/ChatWindow.qml tests/check_phase_2_4_chat_compact_bubble.py
git commit -m "fix: 重做展开态聊天窗外观"
```

---

### Task 6: Speech Bubble Size, Tail, and Hover Controls

**Files:**
- Modify: `apps/desktop/qml/ChatBubbleWindow.qml`
- Modify: `tests/check_phase_2_4_chat_compact_bubble.py`

- [ ] **Step 1: Add failing contract checks for bubble polish**

In `tests/check_phase_2_4_chat_compact_bubble.py`, replace old bubble UI tokens for text glyphs with:

```python
    for token in [
        "readonly property int maxBubbleWidth: 320",
        "readonly property int maxBodyHeight: 340",
        "property int tailX: Math.round(width / 2)",
        "nextPlacement.tailX",
        "qrc:/ui-icons/maximize-2.svg",
        "qrc:/ui-icons/x.svg",
        "anchors.topMargin: bubbleWindow.bodyTop + 14",
        "anchors.rightMargin: 16",
        "bubbleHover.hovered ? 0.82 : 0",
        "ctx.lineTo(tailX - tailHalf, bodyBottom)",
        "ctx.lineTo(tailX, h - 4)",
        "ctx.lineTo(tailX + tailHalf, bodyBottom)",
    ]:
        require(token in bubble_qml, f"ChatBubbleWindow.qml missing bubble polish token {token}")
```

- [ ] **Step 2: Run the contract check and verify it fails**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
```

Expected failure:

```text
AssertionError: ChatBubbleWindow.qml missing bubble polish token readonly property int maxBubbleWidth: 320
```

- [ ] **Step 3: Adjust bubble dimensions**

In `apps/desktop/qml/ChatBubbleWindow.qml`, replace dimension properties with:

```qml
    readonly property int bubbleMargin: 12
    readonly property int contentHorizontalPadding: 24
    readonly property int contentVerticalPadding: 22
    readonly property int pointerExtent: 30
    readonly property int maxBubbleWidth: 320
    readonly property int minBubbleWidth: 190
    readonly property int maxBodyHeight: 340
    readonly property int bodyWidth: Math.max(minBubbleWidth,
            Math.min(maxBubbleWidth, messageMeasure.contentWidth + contentHorizontalPadding * 2))
    readonly property int bodyHeight: Math.max(76,
            Math.min(maxBodyHeight, messageMeasure.contentHeight + contentVerticalPadding * 2))
    readonly property int bodyTop: pointerPlacement.startsWith("top") ? pointerExtent : 2
    readonly property int bodyBottom: pointerPlacement.startsWith("bottom") ? height - pointerExtent : height - 2
    property int tailX: Math.round(width / 2)
```

- [ ] **Step 4: Read tailX from placement**

In `updatePlacement()`, add:

```qml
        tailX = Math.max(42, Math.min(width - 42, nextPlacement.tailX || Math.round(width / 2)))
```

Keep:

```qml
        pointerPlacement = nextPlacement.pointer || "bottomLeft"
```

- [ ] **Step 5: Replace the Canvas tail path**

Inside `Canvas.onPaint`, replace the static tail coordinate block with:

```qml
                const tailX = Math.max(42, Math.min(w - 42, bubbleWindow.tailX))
                const tailHalf = 18
                const bodyTop = bubbleWindow.bodyTop
                const bodyBottom = bubbleWindow.bodyBottom
```

Use the following top-tail path:

```qml
                if (bubbleWindow.pointerPlacement === "topLeft"
                        || bubbleWindow.pointerPlacement === "topRight") {
                    ctx.lineTo(tailX - tailHalf, bodyTop)
                    ctx.lineTo(tailX, 4)
                    ctx.lineTo(tailX + tailHalf, bodyTop)
                }
```

Use the following bottom-tail path:

```qml
                if (bubbleWindow.pointerPlacement === "bottomRight"
                        || bubbleWindow.pointerPlacement === "bottomLeft") {
                    ctx.lineTo(tailX + tailHalf, bodyBottom)
                    ctx.lineTo(tailX, h - 4)
                    ctx.lineTo(tailX - tailHalf, bodyBottom)
                }
```

Keep fill and stroke as one continuous closed path:

```qml
                ctx.fillStyle = "#fffdf8"
                ctx.fill()
                ctx.lineWidth = 3
                ctx.strokeStyle = "#6f6a63"
                ctx.stroke()
```

- [ ] **Step 6: Replace bubble action glyphs with icon buttons**

Replace the two `ToolButton` controls in `bubbleActions` with:

```qml
            MilesIconButton {
                id: expandButton

                iconSource: "qrc:/ui-icons/maximize-2.svg"
                iconSize: 16
                width: 22
                height: 22
                tooltipText: "展开聊天"
                showHoverFill: false
                onClicked: App.ChatController.openWindow()
            }

            MilesIconButton {
                id: closeBubbleButton

                iconSource: "qrc:/ui-icons/x.svg"
                iconSize: 16
                width: 22
                height: 22
                tooltipText: "隐藏气泡"
                showHoverFill: false
                onClicked: bubbleWindow.hideCurrentBubble()
            }
```

Set `bubbleActions` margins and opacity:

```qml
            anchors.topMargin: bubbleWindow.bodyTop + 14
            anchors.rightMargin: 16
            spacing: 8
            opacity: bubbleHover.hovered ? 0.82 : 0
```

- [ ] **Step 7: Make long text preserve latest content**

In `bubbleText.onTextChanged`, set cursor to the end whenever the bubble is not being actively selected:

```qml
                onTextChanged: {
                    if (!bubbleHover.hovered && !activeFocus) {
                        cursorPosition = text.length
                    }
                }
```

Keep `ScrollView` clipping so the newest text remains visible when max height is reached.

- [ ] **Step 8: Run focused verification**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
cmake --build build --target MilesEdgeworthDesktop
```

Expected:

```text
phase 2.4 compact chat bubble contract ok
[100%] Built target MilesEdgeworthDesktop
```

- [ ] **Step 9: Commit**

Run:

```bash
git add apps/desktop/qml/ChatBubbleWindow.qml tests/check_phase_2_4_chat_compact_bubble.py
git commit -m "fix: 优化桌宠回复气泡视觉"
```

---

### Task 7: Manual Visual Verification

**Files:**
- Read-only unless verification exposes a defect. If a defect is found, fix only the files touched by earlier tasks and commit with a targeted message.

- [ ] **Step 1: Build the app**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop
```

Expected:

```text
[100%] Built target MilesEdgeworthDesktop
```

- [ ] **Step 2: Launch the desktop app**

Run:

```bash
open build/apps/desktop/MilesEdgeworthDesktop.app
```

Expected:

- The app launches.
- Miles appears as the desktop pet.
- The compact chat can be opened through the existing app path.

- [ ] **Step 3: Verify compact state**

Check visually:

- Compact chat has no native titlebar and no large outer frame.
- Only the rounded input bar is visible.
- Width is close to `380px`.
- Empty input and short input stay one line.
- Long input grows vertically and then scrolls internally.
- Send icon is centered in its circular button.
- Dragging blank space in the bar moves the bar.
- Clicking inside the text area edits text instead of dragging.
- Expand and close icons remain clickable.

- [ ] **Step 4: Verify expanded state**

Click the compact expand button.

Check visually:

- Expanded chat has a rounded self-drawn shell.
- Top-right icons are gear, refresh, minimize, and x.
- Header icons render as line icons, not filled black blobs.
- Bottom composer has no extra outer border.
- Bottom composer is straight, not visually tilted.
- Send/stop icon is centered.
- Assistant and user messages remain readable.
- No desktop speech bubble is visible while expanded.

- [ ] **Step 5: Verify bubble state**

Send a message in compact state and watch the assistant reply.

Check visually:

- Bubble appears only outside expanded mode.
- Bubble is closer to visible Miles pixels than the previous transparent-window placement.
- Bubble is not excessively left or right of Miles.
- Bubble max width is near `300-320px`.
- Long reply grows vertically beyond two lines before clipping.
- Bubble tail is cleanly fused with the body.
- Hover shows expand and close icons in the same color and size.
- Icons are inset from the right/top edge.

- [ ] **Step 6: Verify four screen edges**

Move Miles near each screen corner and trigger a compact reply.

Check visually:

- Top-left pet: bubble appears lower/right with top-left tail.
- Top-right pet: bubble appears lower/left with top-right tail.
- Bottom-left pet: bubble appears upper/right with bottom-left tail.
- Bottom-right pet: bubble appears upper/left with bottom-right tail.
- Bubble stays inside the screen available geometry.

- [ ] **Step 7: Quit the app**

Quit from the tray/menu or run:

```bash
pkill -f MilesEdgeworthDesktop
```

Expected: the desktop app exits.

- [ ] **Step 8: Commit visual fixes if any**

If Task 7 required code changes, run focused tests from the affected task and commit:

```bash
git add <changed-files>
git commit -m "fix: 修正迷你聊天视觉细节"
```

If no code changed, do not commit.

---

### Task 8: Full Verification

**Files:**
- Read-only.

- [ ] **Step 1: Run whitespace check**

Run:

```bash
git diff --check
```

Expected: no output and exit code `0`.

- [ ] **Step 2: Build all relevant targets**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop ChatBubblePlacementSmoke PetVisibleBoundsSmoke ChatControllerSmoke ChatTextPacerSmoke
```

Expected:

```text
[100%] Built target MilesEdgeworthDesktop
[100%] Built target ChatBubblePlacementSmoke
[100%] Built target PetVisibleBoundsSmoke
[100%] Built target ChatControllerSmoke
[100%] Built target ChatTextPacerSmoke
```

- [ ] **Step 3: Run focused tests**

Run:

```bash
ctest --test-dir build -R "chat_bubble_placement_smoke|pet_visible_bounds_smoke|chat_controller_smoke|chat_text_pacer_smoke|check_phase_2_4_chat_compact_bubble|check_phase_2_2_settings" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 4: Run the full test suite**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 5: Confirm final git state**

Run:

```bash
git status --short --branch
git log --oneline -8
```

Expected:

- Working tree is clean.
- Recent commits include the targeted mini chat polish commits.

---

## Self-Review

Spec coverage:

- Compact no outer frame: Task 4.
- Compact drag on non-interactive areas: Task 4.
- Lucide SVG icons and no black fill: Task 1, Task 4, Task 5, Task 6.
- Expanded self-drawn shell and no extra composer border: Task 5.
- Bubble only outside expanded mode: existing behavior preserved, Task 6 verifies no regression.
- Visible GIF pixel bounds: Task 2.
- Centered, closer bubble placement and four-edge clamp: Task 3, Task 7.
- Bubble width/height/text capacity: Task 6.
- Continuous bubble path and hover controls: Task 6.
- Streaming and controller untouched: file exclusions and focused tests in Task 8.

Placeholder scan:

- No placeholder markers or vague "add tests" steps remain.

Type consistency:

- `ChatBubblePlacementResult.tailX` is defined in Task 3 and read in `DesktopShellController.placeChatBubble`.
- `DesktopShellController::setPetVisibleLocalBounds` is defined in Task 2 and called from `PetSurfaceWindow`.
- `visibleBoundsFromImage` is defined in Task 2 and used by both the smoke test and `PetSurfaceWindow`.
- QML icon resources use the `/ui-icons` prefix consistently.
