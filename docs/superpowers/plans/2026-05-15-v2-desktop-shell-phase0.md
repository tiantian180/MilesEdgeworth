# V2 Desktop Shell Phase 0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first runnable Qt 6/QML v2 desktop shell that shows Miles as a transparent, frameless, draggable, always-on-top pet window on macOS.

**Architecture:** This plan creates a new `apps/desktop` project without moving legacy Qt Widgets files. The new app uses QML for the pet window and a small C++ platform helper for macOS window behavior. It intentionally avoids Go sidecar, ChatWindow, Dashboard, and the full Pet Runtime until the desktop shell is proven.

**Tech Stack:** CMake, Ninja, Qt 6.11, Qt Quick/QML, C++17, Objective-C++ for macOS window-level integration.

---

## Scope

This plan implements only Phase 0 from `docs/v2/architecture.md`:

```text
Qt 6/QML app skeleton
transparent frameless pet window
old Miles stand animation loaded through a v2 resource file
manual dragging
macOS always-on-top / Spaces behavior helper
build and launch commands
```

This plan does not implement:

```text
Go sidecar
chat
dashboard
SQLite
AG-UI event stream
full animation orchestrator
skin manifest parser
Windows/Linux packaging
```

## File Map

Create these files:

```text
apps/desktop/CMakeLists.txt
apps/desktop/resources/pet_assets.qrc
apps/desktop/src/main.cpp
apps/desktop/src/platform/MacPetWindowBehavior.h
apps/desktop/src/platform/MacPetWindowBehavior.mm
apps/desktop/qml/PetWindow.qml
docs/v2/phase0-desktop-shell.md
```

Modify these files:

```text
.gitignore
```

## Task 0: Ignore Local Brainstorming Artifacts

**Files:**

- Modify: `.gitignore`

- [ ] **Step 1: Add `.superpowers/` to `.gitignore`**

Append this block:

```gitignore
# Local brainstorming / visual companion artifacts
.superpowers/
```

- [ ] **Step 2: Check git status**

Run:

```bash
git status --short
```

Expected:

```text
The .superpowers/ directory is not listed.
```

- [ ] **Step 3: Commit the gitignore change**

```bash
git add .gitignore
git commit -m "chore: ignore local brainstorming artifacts"
```

## Task 1: Add Desktop Project Skeleton

**Files:**

- Create: `apps/desktop/CMakeLists.txt`
- Create: `apps/desktop/src/main.cpp`
- Create: `apps/desktop/resources/pet_assets.qrc`
- Create: `apps/desktop/qml/PetWindow.qml`

- [ ] **Step 1: Create `apps/desktop/CMakeLists.txt`**

Add this exact file:

```cmake
cmake_minimum_required(VERSION 3.21)

project(MilesEdgeworthDesktop VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.5)

set(DESKTOP_SOURCES
    src/main.cpp
    resources/pet_assets.qrc
)

qt_add_executable(MilesEdgeworthDesktop
    MANUAL_FINALIZATION
    ${DESKTOP_SOURCES}
)

qt_add_qml_module(MilesEdgeworthDesktop
    URI MilesEdgeworth
    VERSION 1.0
    QML_FILES
        qml/PetWindow.qml
)

target_include_directories(MilesEdgeworthDesktop
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(MilesEdgeworthDesktop
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
)

set_target_properties(MilesEdgeworthDesktop PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "MilesEdgeworth v2"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.tian.MilesEdgeworth.v2"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(MilesEdgeworthDesktop)
```

- [ ] **Step 2: Create `apps/desktop/resources/pet_assets.qrc`**

Use QRC aliases so v2 code does not depend on legacy numeric filenames directly:

```xml
<RCC>
  <qresource prefix="/pet">
    <file alias="stand-right.gif">../../../gifs/stand/0.gif</file>
    <file alias="stand-left.gif">../../../gifs/stand/1.gif</file>
  </qresource>
</RCC>
```

- [ ] **Step 3: Create placeholder `apps/desktop/qml/PetWindow.qml`**

Create a minimal QML file so CMake can configure and the app can launch before the real pet UI is added.

```qml
import QtQuick
import QtQuick.Window

Window {
    width: 240
    height: 240
    visible: true
    color: "transparent"
    title: "MilesEdgeworth v2"
}
```

- [ ] **Step 4: Create minimal `apps/desktop/src/main.cpp`**

This file starts the QML app. macOS-specific behavior is added in Task 3.

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("MilesEdgeworth", "PetWindow");

    return app.exec();
}
```

- [ ] **Step 5: Configure the new project**

Run:

```bash
cmake -S apps/desktop -B /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
```

Expected:

```text
-- Configuring done
-- Generating done
-- Build files have been written to: /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

If `CMAKE_PREFIX_PATH=/opt/homebrew` is wrong on an Intel Mac, retry with:

```bash
cmake -S apps/desktop -B /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build -G Ninja -DCMAKE_PREFIX_PATH=/usr/local
```

- [ ] **Step 6: Build**

Run:

```bash
cmake --build /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

Expected:

```text
[100%] Built target MilesEdgeworthDesktop
```

This verifies the v2 desktop project can compile before adding the real pet UI.

- [ ] **Step 7: Commit the skeleton**

```bash
git add apps/desktop/CMakeLists.txt apps/desktop/src/main.cpp apps/desktop/resources/pet_assets.qrc apps/desktop/qml/PetWindow.qml
git commit -m "build: add v2 desktop shell project"
```

## Task 2: Add Transparent QML Pet Window

**Files:**

- Modify: `apps/desktop/qml/PetWindow.qml`

- [ ] **Step 1: Replace `apps/desktop/qml/PetWindow.qml`**

This QML file shows the legacy stand-right animation, uses transparent window color, and supports manual dragging.

```qml
import QtQuick
import QtQuick.Window

Window {
    id: petWindow

    width: 240
    height: 240
    visible: true
    color: "transparent"
    title: "MilesEdgeworth v2"

    flags: Qt.FramelessWindowHint
           | Qt.WindowStaysOnTopHint
           | Qt.Tool
           | Qt.NoDropShadowWindowHint

    // 当前阶段先直接显示一个旧版站立动画。
    // 后续 Pet Runtime 会接管动画选择，不再让 QML 写死资源路径。
    AnimatedImage {
        id: pet

        anchors.centerIn: parent
        source: "qrc:/pet/stand-right.gif"
        cache: false
        playing: true
        fillMode: Image.PreserveAspectFit
        width: 200
        height: 200
    }

    // Phase 0 先用最容易读懂的拖拽逻辑。
    // 后续如果要做到像旧版一样的像素级点击区域，需要交给 Pet Runtime 和 hit mask。
    MouseArea {
        id: dragArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton

        property real pressX: 0
        property real pressY: 0

        onPressed: function(mouse) {
            pressX = mouse.x
            pressY = mouse.y
        }

        onPositionChanged: function(mouse) {
            if ((mouse.buttons & Qt.LeftButton) === 0) {
                return
            }

            petWindow.x += mouse.x - pressX
            petWindow.y += mouse.y - pressY
        }
    }
}
```

- [ ] **Step 2: Build**

Run:

```bash
cmake --build /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

Expected:

```text
[100%] Built target MilesEdgeworthDesktop
```

- [ ] **Step 3: Launch the app**

Run:

```bash
open /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build/MilesEdgeworthDesktop.app
```

Expected:

```text
The app opens a frameless transparent pet window showing Miles standing.
The window can be dragged with the left mouse button.
```

- [ ] **Step 4: Manual visual checks**

Check these behaviors:

```text
The window background is transparent.
Miles animation is visible.
There is no normal title bar.
Dragging moves the window.
The app appears in front of normal windows.
```

- [ ] **Step 5: Commit the QML pet window**

```bash
git add apps/desktop/qml/PetWindow.qml
git commit -m "feat: add v2 transparent pet window"
```

## Task 3: Add macOS Pet Window Behavior

**Files:**

- Create: `apps/desktop/src/platform/MacPetWindowBehavior.h`
- Create: `apps/desktop/src/platform/MacPetWindowBehavior.mm`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/src/main.cpp`

- [ ] **Step 1: Update `apps/desktop/CMakeLists.txt` for macOS sources**

Insert this block after the `set(DESKTOP_SOURCES ...)` section and before `qt_add_executable(...)`:

```cmake
if(APPLE)
    enable_language(OBJCXX)
    list(APPEND DESKTOP_SOURCES
        src/platform/MacPetWindowBehavior.h
        src/platform/MacPetWindowBehavior.mm
    )
endif()
```

- [ ] **Step 2: Create `apps/desktop/src/platform/MacPetWindowBehavior.h`**

```cpp
#pragma once

class QWindow;

// 给 macOS 桌宠窗口追加平台行为：
// 1. 不因为应用失焦而隐藏。
// 2. 尽量保持在所有 Space / 全屏窗口之上。
// 3. 使用透明背景，避免普通应用窗口外观。
//
// 这个函数只在 macOS 编译。其他平台会在调用处走空实现。
void applyMacPetWindowBehavior(QWindow *window);
```

- [ ] **Step 3: Create `apps/desktop/src/platform/MacPetWindowBehavior.mm`**

This is adapted from the proven v1 macOS helper, but accepts `QWindow*` for QML.

```objc
#include "platform/MacPetWindowBehavior.h"

#include <QWindow>
#include <dlfcn.h>

#import <Cocoa/Cocoa.h>

namespace {
using SLSMainConnectionIDFn = int (*)();
using SLSSpaceCreateFn = int (*)(int, int, int);
using SLSSpaceSetAbsoluteLevelFn = int (*)(int, int, int);
using SLSShowSpacesFn = int (*)(int, void *);
using SLSSpaceAddWindowsAndRemoveFromSpacesFn = int (*)(int, int, void *, int);

struct SkyLightSpace {
    int connection = 0;
    int space = 0;
    SLSSpaceAddWindowsAndRemoveFromSpacesFn addWindowsAndRemoveFromSpaces = nullptr;
    bool available = false;
};

SkyLightSpace &stationarySkyLightSpace()
{
    static SkyLightSpace state;
    static bool initialized = false;
    if (initialized) {
        return state;
    }
    initialized = true;

    void *skyLight = dlopen("/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/SkyLight", RTLD_LAZY);
    if (skyLight == nullptr) {
        return state;
    }

    auto mainConnection = reinterpret_cast<SLSMainConnectionIDFn>(dlsym(skyLight, "SLSMainConnectionID"));
    auto spaceCreate = reinterpret_cast<SLSSpaceCreateFn>(dlsym(skyLight, "SLSSpaceCreate"));
    auto spaceSetAbsoluteLevel = reinterpret_cast<SLSSpaceSetAbsoluteLevelFn>(dlsym(skyLight, "SLSSpaceSetAbsoluteLevel"));
    auto showSpaces = reinterpret_cast<SLSShowSpacesFn>(dlsym(skyLight, "SLSShowSpaces"));
    auto addWindowsAndRemoveFromSpaces = reinterpret_cast<SLSSpaceAddWindowsAndRemoveFromSpacesFn>(
        dlsym(skyLight, "SLSSpaceAddWindowsAndRemoveFromSpaces"));

    if (mainConnection == nullptr
        || spaceCreate == nullptr
        || spaceSetAbsoluteLevel == nullptr
        || showSpaces == nullptr
        || addWindowsAndRemoveFromSpaces == nullptr) {
        return state;
    }

    state.connection = mainConnection();
    state.space = spaceCreate(state.connection, 1, 0);
    if (state.connection == 0 || state.space == 0) {
        return state;
    }

    NSArray *spaces = @[ @(state.space) ];
    spaceSetAbsoluteLevel(state.connection, state.space, 100);
    showSpaces(state.connection, (__bridge void *)spaces);

    state.addWindowsAndRemoveFromSpaces = addWindowsAndRemoveFromSpaces;
    state.available = true;
    return state;
}

void moveWindowToStationarySkyLightSpace(NSWindow *window)
{
    if (window == nil) {
        return;
    }

    SkyLightSpace &state = stationarySkyLightSpace();
    if (!state.available || state.addWindowsAndRemoveFromSpaces == nullptr) {
        return;
    }

    const NSInteger windowNumber = [window windowNumber];
    if (windowNumber <= 0) {
        return;
    }

    NSArray *windows = @[ @(windowNumber) ];
    state.addWindowsAndRemoveFromSpaces(state.connection, state.space, (__bridge void *)windows, 7);
}
}

void applyMacPetWindowBehavior(QWindow *window)
{
    if (window == nullptr) {
        return;
    }

    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

    NSView *view = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nativeWindow = [view window];
    if (nativeWindow == nil) {
        return;
    }

    [nativeWindow setHidesOnDeactivate:NO];
    [nativeWindow setCanHide:NO];
    [nativeWindow setRestorable:NO];
    [nativeWindow setOpaque:NO];
    [nativeWindow setBackgroundColor:[NSColor clearColor]];
    [nativeWindow setMovable:NO];
    [nativeWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];

    NSWindowCollectionBehavior behavior = [nativeWindow collectionBehavior];
    behavior &= ~(NSWindowCollectionBehaviorMoveToActiveSpace
                  | NSWindowCollectionBehaviorManaged
                  | NSWindowCollectionBehaviorTransient
                  | NSWindowCollectionBehaviorParticipatesInCycle
                  | NSWindowCollectionBehaviorFullScreenPrimary
                  | NSWindowCollectionBehaviorFullScreenNone
                  | NSWindowCollectionBehaviorFullScreenAllowsTiling);
    if (@available(macOS 13.0, *)) {
        behavior &= ~NSWindowCollectionBehaviorCanJoinAllApplications;
    }
    behavior |= NSWindowCollectionBehaviorCanJoinAllSpaces
                | NSWindowCollectionBehaviorStationary
                | NSWindowCollectionBehaviorFullScreenAuxiliary
                | NSWindowCollectionBehaviorIgnoresCycle
                | NSWindowCollectionBehaviorFullScreenDisallowsTiling;
    [nativeWindow setCollectionBehavior:behavior];

    [nativeWindow setLevel:CGWindowLevelForKey(kCGAssistiveTechHighWindowLevelKey)];
    moveWindowToStationarySkyLightSpace(nativeWindow);
    [nativeWindow orderFrontRegardless];
}
```

- [ ] **Step 4: Update `apps/desktop/src/main.cpp`**

Replace the file with:

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QWindow>

#ifdef Q_OS_MACOS
#include "platform/MacPetWindowBehavior.h"
#endif

namespace {
void applyPlatformPetWindowBehavior(QQmlApplicationEngine &engine)
{
    if (engine.rootObjects().isEmpty()) {
        return;
    }

    auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    if (window == nullptr) {
        return;
    }

#ifdef Q_OS_MACOS
    applyMacPetWindowBehavior(window);
#else
    window->setFlag(Qt::WindowStaysOnTopHint, true);
#endif
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("MilesEdgeworth", "PetWindow");

    // 等 QML Window 创建完 native handle 后，再追加平台级桌宠窗口行为。
    QTimer::singleShot(0, &engine, [&engine]() {
        applyPlatformPetWindowBehavior(engine);
    });

    return app.exec();
}
```

- [ ] **Step 5: Build**

Run:

```bash
cmake --build /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

Expected:

```text
[100%] Built target MilesEdgeworthDesktop
```

- [ ] **Step 6: Launch and verify macOS Spaces behavior**

Run:

```bash
open /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build/MilesEdgeworthDesktop.app
```

Manual checks:

```text
Click another app: Miles remains visible.
Switch desktop Spaces: Miles remains visible during and after the switch.
Open a full-screen app: Miles remains above it.
Drag Miles: the window position changes normally.
```

- [ ] **Step 7: Commit macOS behavior**

```bash
git add apps/desktop/CMakeLists.txt apps/desktop/src/main.cpp apps/desktop/src/platform/MacPetWindowBehavior.h apps/desktop/src/platform/MacPetWindowBehavior.mm
git commit -m "feat: add macOS pet window behavior"
```

## Task 4: Document Phase 0 Build and Verification

**Files:**

- Create: `docs/v2/phase0-desktop-shell.md`

- [ ] **Step 1: Create `docs/v2/phase0-desktop-shell.md`**

````markdown
# Phase 0 Desktop Shell Verification

This document records how to build and manually verify the v2 Qt/QML desktop shell.

## Build

Apple Silicon:

```bash
cmake -S apps/desktop -B /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

Intel Mac:

```bash
cmake -S apps/desktop -B /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build -G Ninja -DCMAKE_PREFIX_PATH=/usr/local
cmake --build /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

## Run

```bash
open /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build/MilesEdgeworthDesktop.app
```

## Manual Checks

- Miles appears in a transparent frameless window.
- The window can be dragged.
- The app remains visible after clicking another app.
- The app remains visible while switching Spaces.
- The app remains visible above a full-screen app.
- The animation loads from the v2 resource alias `qrc:/pet/stand-right.gif`.

## Known Limits

- The clickable region is still rectangular in Phase 0.
- Pet Runtime is not implemented in Phase 0.
- Only the stand-right and stand-left legacy GIF aliases are registered.
- Windows and Linux behavior will be verified in a separate cross-platform pass.
````

- [ ] **Step 2: Verify docs have no placeholder markers**

Run:

```bash
rg -n "占位符|未明确" docs/v2/phase0-desktop-shell.md
```

Expected:

```text
No output.
```

- [ ] **Step 3: Commit verification docs**

```bash
git add docs/v2/phase0-desktop-shell.md
git commit -m "docs: add phase 0 desktop shell verification"
```

## Task 5: Final Phase 0 Verification

**Files:**

- No new files.

- [ ] **Step 1: Clean configure**

Run:

```bash
rm -rf /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
cmake -S apps/desktop -B /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
```

Expected:

```text
-- Configuring done
-- Generating done
-- Build files have been written to: /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

- [ ] **Step 2: Build**

Run:

```bash
cmake --build /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

Expected:

```text
[100%] Built target MilesEdgeworthDesktop
```

- [ ] **Step 3: Launch**

Run:

```bash
open /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build/MilesEdgeworthDesktop.app
```

Expected:

```text
MilesEdgeworthDesktop.app launches and shows Miles in a transparent draggable pet window.
```

- [ ] **Step 4: Check git status**

Run:

```bash
git status --short
```

Expected:

```text
No output.
```

- [ ] **Step 5: Push branch**

Run:

```bash
git push -u origin v2-ai-pet
```

Expected:

```text
branch 'v2-ai-pet' set up to track 'origin/v2-ai-pet'
```
