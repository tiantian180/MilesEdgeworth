# Mini Chat UI Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Polish the compact chat UI, expanded chat UI, and pet reply bubble according to `docs/superpowers/specs/2026-05-25-mini-chat-ui-redesign.md`.

**Architecture:** Keep `ChatController` as the only source of chat text, sending state, provider state, and streaming pace. Put testable bubble placement math in a small C++ helper, expose the result through `DesktopShellController`, and keep all visual layout in QML. Refactor the chat composer into a reusable QML component so compact and expanded modes share one input implementation.

**Tech Stack:** Qt 6.5+ / QML `ApplicationWindow`, `TextArea`, `Canvas`, `HoverHandler`, C++17 + Qt Core geometry helpers, CMake / Ninja / CTest.

---

## Scope Check

This plan implements one UI refinement over the existing compact chat branch.

Included:

- Smaller draggable compact input bar.
- Shared compact/expanded composer style and behavior.
- Send/stop button inside the composer lower-right corner.
- Expanded mode using the same composer while hiding the pet reply bubble.
- Comic reply bubble drawn as one continuous Canvas path.
- Hover-only same-color bubble glyph controls.
- Four-direction bubble placement around the pet with screen-edge clamping.
- Bubble auto-hide timing based on final assistant text length and pause-on-hover behavior.

Excluded:

- `ChatController` streaming, segment queue, `ChatTextPacer`, SSE parsing, sidecar API, model provider logic, and conversation persistence.
- New icon asset dependencies.
- Pet animation timing or Phase 2.4 animation behavior.

## Required Reading

Read these files before editing:

- `docs/superpowers/specs/2026-05-25-mini-chat-ui-redesign.md`
- `docs/v2/设计方案/总体架构设计.md` §4, for Qt desktop versus Go sidecar responsibility.
- `docs/v2/设计方案/AI 聊天动画编排设计.md` §3, §6, for the rule that UI mirrors already-paced `ChatController.messages`.
- `apps/desktop/qml/ChatWindow.qml`
- `apps/desktop/qml/ChatBubbleWindow.qml`
- `apps/desktop/src/DesktopShellController.{h,cpp}`
- `apps/desktop/CMakeLists.txt`

Context7 Qt check used for this plan:

- `QtQuick.Canvas` supports `getContext("2d")`, `beginPath()`, `quadraticCurveTo()`, `closePath()`, `fill()`, and `stroke()`, suitable for one continuous comic speech-bubble path.
- `TextArea` supports `contentHeight`, `wrapMode`, and padding for auto-growing input.
- `HoverHandler` and `MouseArea.hoverEnabled` support hover-only controls.
- `Timer` supports one-shot delayed auto-hide.

## File Structure

Create:

- `apps/desktop/src/chat/ChatBubblePlacement.h`
  - Pure C++ declarations for `ChatBubblePlacementResult` and `placeChatBubble(...)`.
- `apps/desktop/src/chat/ChatBubblePlacement.cpp`
  - Testable geometry algorithm for four-direction bubble placement and final screen clamping.
- `apps/desktop/tests/chat_bubble_placement_smoke.cpp`
  - C++ smoke test for the four pet corner positions and final clamp behavior.
- `apps/desktop/qml/ChatComposer.qml`
  - Shared compact/expanded composer with internal send/stop button, lightweight expand/close glyphs, auto-growing input, and submit handling hooks.

Modify:

- `apps/desktop/CMakeLists.txt`
  - Register the new C++ helper, smoke test, and QML component.
- `apps/desktop/src/DesktopShellController.h`
  - Add `chatWindowExpanded` UI state and `placeChatBubble(...)` QML bridge.
- `apps/desktop/src/DesktopShellController.cpp`
  - Implement `chatWindowExpanded` setter and bridge to the pure placement helper.
- `apps/desktop/qml/ChatWindow.qml`
  - Replace the inline composer row with `ChatComposer`, make compact mode smaller, add compact drag behavior, and report expanded state to `DesktopShell`.
- `apps/desktop/qml/ChatBubbleWindow.qml`
  - Use the placement bridge, hide while expanded mode is visible, draw one continuous comic bubble path, and adjust hover controls/auto-hide.

Not modified:

- `apps/desktop/src/chat/ChatController.{h,cpp}`
- `apps/desktop/src/chat/ChatTextPacer.{h,cpp}`
- `apps/agent-core/**`
- `docs/v2/设计方案/**`
- `README.md`, unless implementation reveals a new user-facing build or usage change.

---

## Task 0: Preflight

**Files:**

- Read-only.

- [ ] **Step 1: Confirm branch and clean tree**

Run:

```bash
git status --short --branch
```

Expected: the first line starts with:

```text
## feature/chat-compact-bubble...origin/feature/chat-compact-bubble
```

Working tree must be clean. Any high ahead count is against the old remote feature branch; relative to `origin/main`, this branch should only contain local design/plan commits.

- [ ] **Step 2: Confirm this branch is based on main**

Run:

```bash
git rev-list --left-right --count origin/main...HEAD
```

Expected:

```text
0	3
```

If the right-side count is higher because this plan has already been committed, accept that exact higher count only when `git log --oneline origin/main..HEAD` contains documentation commits for this UI redesign. The left-side count must be `0`.

- [ ] **Step 3: Build current baseline**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target MilesEdgeworthDesktop ChatControllerSmoke ChatTextPacerSmoke
ctest --test-dir build -R "chat_controller_smoke|chat_text_pacer_smoke" --output-on-failure
```

Expected: build succeeds and both tests pass.

- [ ] **Step 4: No commit**

This task changes nothing.

---

## Task 1: Bubble Placement Helper

**Files:**

- Create: `apps/desktop/src/chat/ChatBubblePlacement.h`
- Create: `apps/desktop/src/chat/ChatBubblePlacement.cpp`
- Create: `apps/desktop/tests/chat_bubble_placement_smoke.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Write the failing placement smoke test**

Create `apps/desktop/tests/chat_bubble_placement_smoke.cpp`:

```cpp
#include "chat/ChatBubblePlacement.h"

#include <QRect>
#include <QSize>

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
    const QRect screen(0, 0, 1000, 800);
    const QSize bubbleSize(280, 120);
    const int margin = 12;

    {
        const QRect pet(24, 32, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("topLeft"),
                "top-left pet should use a top-left pointer");
        require(result.topLeft.x() >= pet.right(),
                "top-left pet should place the bubble to the pet's right");
        require(result.topLeft.y() >= pet.bottom(),
                "top-left pet should place the bubble below the pet");
    }

    {
        const QRect pet(880, 32, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("topRight"),
                "top-right pet should use a top-right pointer");
        require(result.topLeft.x() + bubbleSize.width() <= pet.left(),
                "top-right pet should place the bubble to the pet's left");
        require(result.topLeft.y() >= pet.bottom(),
                "top-right pet should place the bubble below the pet");
    }

    {
        const QRect pet(24, 636, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("bottomLeft"),
                "bottom-left pet should use a bottom-left pointer");
        require(result.topLeft.x() >= pet.right(),
                "bottom-left pet should place the bubble to the pet's right");
        require(result.topLeft.y() + bubbleSize.height() <= pet.top(),
                "bottom-left pet should place the bubble above the pet");
    }

    {
        const QRect pet(880, 636, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("bottomRight"),
                "bottom-right pet should use a bottom-right pointer");
        require(result.topLeft.x() + bubbleSize.width() <= pet.left(),
                "bottom-right pet should place the bubble to the pet's left");
        require(result.topLeft.y() + bubbleSize.height() <= pet.top(),
                "bottom-right pet should place the bubble above the pet");
    }

    {
        const QRect tinyScreen(0, 0, 240, 160);
        const QRect pet(90, 70, 80, 80);
        const QSize largeBubble(300, 220);
        const ChatBubblePlacementResult result = placeChatBubble(pet, tinyScreen, largeBubble, margin);
        require(result.topLeft.x() == margin,
                "oversized bubble should clamp to the available left margin");
        require(result.topLeft.y() == margin,
                "oversized bubble should clamp to the available top margin");
    }

    return 0;
}
```

- [ ] **Step 2: Register the missing test target**

In `apps/desktop/CMakeLists.txt`, add `src/chat/ChatBubblePlacement.cpp` and `src/chat/ChatBubblePlacement.h` to `DESKTOP_SOURCES` after `ChatTextPacer.h`.

In the `if(BUILD_TESTING)` block, after `ChatTextPacerSmoke`, insert:

```cmake
    add_executable(ChatBubblePlacementSmoke
        tests/chat_bubble_placement_smoke.cpp
        src/chat/ChatBubblePlacement.cpp
        src/chat/ChatBubblePlacement.h
    )

    target_include_directories(ChatBubblePlacementSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ChatBubblePlacementSmoke
        PRIVATE
            Qt6::Core
    )

    add_test(NAME chat_bubble_placement_smoke COMMAND ChatBubblePlacementSmoke)
```

- [ ] **Step 3: Run the test and verify it fails**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target ChatBubblePlacementSmoke
```

Expected: FAIL because `apps/desktop/src/chat/ChatBubblePlacement.h` does not exist yet.

- [ ] **Step 4: Add the placement helper header**

Create `apps/desktop/src/chat/ChatBubblePlacement.h`:

```cpp
#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

struct ChatBubblePlacementResult
{
    QPoint topLeft;
    QString pointer;
};

ChatBubblePlacementResult placeChatBubble(
    const QRect &petGeometry,
    const QRect &availableGeometry,
    const QSize &bubbleSize,
    int margin
);
```

- [ ] **Step 5: Add the placement helper implementation**

Create `apps/desktop/src/chat/ChatBubblePlacement.cpp`:

```cpp
#include "chat/ChatBubblePlacement.h"

#include <QtGlobal>

namespace {
int clampCoordinate(int value, int minimum, int maximum)
{
    return maximum >= minimum ? qBound(minimum, value, maximum) : minimum;
}

int safeDimension(int value)
{
    return qMax(1, value);
}
} // namespace

ChatBubblePlacementResult placeChatBubble(
    const QRect &petGeometry,
    const QRect &availableGeometry,
    const QSize &bubbleSize,
    int margin
)
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

    const QPoint petCenter = pet.center();
    const QPoint screenCenter = available.center();
    const bool petOnLeft = petCenter.x() < screenCenter.x();
    const bool petOnTop = petCenter.y() < screenCenter.y();

    const int preferredX = petOnLeft
        ? pet.right() + safeMargin
        : pet.left() - bubbleWidth - safeMargin;
    const int preferredY = petOnTop
        ? pet.bottom() + safeMargin
        : pet.top() - bubbleHeight - safeMargin;

    const int minX = available.left() + safeMargin;
    const int minY = available.top() + safeMargin;
    const int maxX = available.right() - bubbleWidth - safeMargin + 1;
    const int maxY = available.bottom() - bubbleHeight - safeMargin + 1;

    ChatBubblePlacementResult result;
    result.topLeft = QPoint(
        clampCoordinate(preferredX, minX, maxX),
        clampCoordinate(preferredY, minY, maxY)
    );

    if (petOnTop && petOnLeft) {
        result.pointer = QStringLiteral("topLeft");
    } else if (petOnTop) {
        result.pointer = QStringLiteral("topRight");
    } else if (petOnLeft) {
        result.pointer = QStringLiteral("bottomLeft");
    } else {
        result.pointer = QStringLiteral("bottomRight");
    }

    return result;
}
```

- [ ] **Step 6: Run the placement test**

Run:

```bash
cmake --build build --target ChatBubblePlacementSmoke
ctest --test-dir build -R chat_bubble_placement_smoke --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add apps/desktop/CMakeLists.txt \
  apps/desktop/src/chat/ChatBubblePlacement.h \
  apps/desktop/src/chat/ChatBubblePlacement.cpp \
  apps/desktop/tests/chat_bubble_placement_smoke.cpp
git commit -m "test: 增加聊天气泡定位测试"
```

---

## Task 2: Shell Bridge For Bubble And Expanded State

**Files:**

- Modify: `apps/desktop/src/DesktopShellController.h`
- Modify: `apps/desktop/src/DesktopShellController.cpp`

- [ ] **Step 1: Add the shell API declarations**

In `apps/desktop/src/DesktopShellController.h`, add:

```cpp
#include <QVariantMap>
```

Add this property after `petScreenAvailableHeight`:

```cpp
    Q_PROPERTY(bool chatWindowExpanded READ chatWindowExpanded NOTIFY chatWindowStateChanged)
```

Add this getter in the public section:

```cpp
    bool chatWindowExpanded() const;
```

Add these invokables in `public slots`:

```cpp
    Q_INVOKABLE void setChatWindowExpanded(bool expanded);
    Q_INVOKABLE QVariantMap placeChatBubble(int bubbleWidth, int bubbleHeight, int margin) const;
```

Add this signal:

```cpp
    void chatWindowStateChanged();
```

Add this private member:

```cpp
    bool m_chatWindowExpanded = false;
```

- [ ] **Step 2: Implement the shell bridge**

In `apps/desktop/src/DesktopShellController.cpp`, add:

```cpp
#include "chat/ChatBubblePlacement.h"
```

After `int DesktopShellController::petScreenAvailableHeight() const`, add:

```cpp
bool DesktopShellController::chatWindowExpanded() const
{
    return m_chatWindowExpanded;
}
```

After `void DesktopShellController::setChatWindowDockVisible(bool visible)`, add:

```cpp
void DesktopShellController::setChatWindowExpanded(bool expanded)
{
    if (m_chatWindowExpanded == expanded) {
        return;
    }

    m_chatWindowExpanded = expanded;
    emit chatWindowStateChanged();
}

QVariantMap DesktopShellController::placeChatBubble(int bubbleWidth, int bubbleHeight, int margin) const
{
    const QRect petGeometry(petWindowX(), petWindowY(), petWindowWidth(), petWindowHeight());
    const ChatBubblePlacementResult placement = ::placeChatBubble(
        petGeometry,
        petScreenAvailableGeometry(),
        QSize(bubbleWidth, bubbleHeight),
        margin
    );

    QVariantMap result;
    result.insert(QStringLiteral("x"), placement.topLeft.x());
    result.insert(QStringLiteral("y"), placement.topLeft.y());
    result.insert(QStringLiteral("pointer"), placement.pointer);
    return result;
}
```

- [ ] **Step 3: Build and verify the bridge compiles**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop ChatBubblePlacementSmoke
ctest --test-dir build -R chat_bubble_placement_smoke --output-on-failure
```

Expected: build succeeds and placement test passes.

- [ ] **Step 4: Commit**

```bash
git add apps/desktop/src/DesktopShellController.h apps/desktop/src/DesktopShellController.cpp
git commit -m "feat: 暴露聊天气泡定位桥接"
```

---

## Task 3: Shared Chat Composer

**Files:**

- Create: `apps/desktop/qml/ChatComposer.qml`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Register the QML component**

In `apps/desktop/CMakeLists.txt`, add `qml/ChatComposer.qml` to the `qt_add_qml_module(... QML_FILES ...)` list before `qml/ChatWindow.qml`.

- [ ] **Step 2: Create the shared composer**

Create `apps/desktop/qml/ChatComposer.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MilesEdgeworth as App

Item {
    id: root

    property bool compactMode: false
    property alias text: input.text
    readonly property bool disconnectedInput: !App.ChatController.sidecarReady && !App.ChatController.sending
    readonly property bool providerConfigured: App.ChatController.providerConfigured
    readonly property bool canType: App.ChatController.sidecarReady && providerConfigured && !App.ChatController.sending
    readonly property bool missingProviderConfig: !providerConfigured && !App.ChatController.sending
    readonly property bool disabledInput: disconnectedInput || missingProviderConfig
    readonly property bool hasText: input.text.trim().length > 0
    readonly property bool canSubmit: App.ChatController.sending
            || (App.ChatController.sidecarReady && providerConfigured && hasText)
    readonly property int inputMinHeight: compactMode ? 44 : 52
    readonly property int inputMaxHeight: compactMode ? 112 : 136
    readonly property int inputTargetHeight: Math.max(inputMinHeight,
            Math.min(inputMaxHeight, Math.ceil(input.contentHeight + 18)))
    readonly property int outerVerticalPadding: compactMode ? 8 : 12
    readonly property int buttonSize: compactMode ? 34 : 38

    implicitWidth: compactMode ? 460 : 520
    implicitHeight: inputTargetHeight + outerVerticalPadding * 2

    signal submitRequested(string text)
    signal cancelRequested()
    signal expandRequested()
    signal closeRequested()

    function clearText() {
        input.text = ""
    }

    function forceInputFocus() {
        input.forceActiveFocus()
    }

    function submitOrCancel() {
        if (App.ChatController.sending) {
            root.cancelRequested()
            return
        }

        const trimmed = input.text.trim()
        if (trimmed.length === 0) {
            return
        }
        root.submitRequested(trimmed)
    }

    RowLayout {
        anchors.fill: parent
        spacing: compactMode ? 6 : 8

        ToolButton {
            id: expandButton

            visible: root.compactMode
            text: "↗"
            font.pixelSize: 26
            Layout.preferredWidth: visible ? 38 : 0
            Layout.preferredHeight: root.inputMinHeight
            ToolTip.visible: hovered
            ToolTip.text: "展开聊天"
            contentItem: Text {
                text: expandButton.text
                color: "#7b746d"
                font: expandButton.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Item {}
            onClicked: root.expandRequested()
        }

        Rectangle {
            id: composerFrame

            Layout.fillWidth: true
            Layout.preferredHeight: root.inputTargetHeight
            radius: compactMode ? 15 : 18
            color: root.disabledInput ? "#fff4ec" : "#fffdf8"
            border.color: root.disabledInput ? "#c66a4b" : (input.activeFocus ? "#9fb4cc" : "#cfd8e3")
            border.width: input.activeFocus || root.disabledInput ? 2 : 1
            clip: true

            TextArea {
                id: input

                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: root.buttonSize + 18
                anchors.topMargin: compactMode ? 5 : 8
                anchors.bottomMargin: compactMode ? 5 : 8
                wrapMode: TextArea.Wrap
                placeholderText: root.missingProviderConfig
                        ? "先点设置填写模型配置"
                        : (root.disconnectedInput ? "未连接，点重连或稍后重试" : "输入消息")
                placeholderTextColor: root.disabledInput ? "#8a4b38" : "#8d8580"
                color: enabled ? "#26201b" : "#6f5545"
                selectionColor: "#b9d0f2"
                selectedTextColor: "#26201b"
                enabled: root.canType
                textFormat: TextEdit.PlainText
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                background: Item {}

                Keys.onPressed: function(event) {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && (event.modifiers & Qt.ShiftModifier) === 0) {
                        event.accepted = true
                        root.submitOrCancel()
                    }
                }
            }

            ToolButton {
                id: sendButton

                width: root.buttonSize
                height: root.buttonSize
                anchors.right: parent.right
                anchors.rightMargin: compactMode ? 8 : 10
                anchors.bottom: parent.bottom
                anchors.bottomMargin: compactMode ? 5 : 8
                enabled: root.canSubmit
                text: App.ChatController.sending ? "■" : "↑"
                font.pixelSize: App.ChatController.sending ? 16 : 22
                font.weight: Font.DemiBold
                ToolTip.visible: hovered
                ToolTip.text: App.ChatController.sending ? "停止回复" : "发送"
                contentItem: Text {
                    text: sendButton.text
                    color: sendButton.enabled ? "#fffdf8" : "#7d746d"
                    font: sendButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 12
                    color: sendButton.enabled
                            ? (sendButton.down ? "#254867" : "#315a7d")
                            : "#ede8df"
                    border.color: sendButton.enabled ? "#254867" : "#d7cec3"
                }
                onClicked: root.submitOrCancel()
            }
        }

        ToolButton {
            id: closeButton

            visible: root.compactMode
            text: "×"
            font.pixelSize: 28
            Layout.preferredWidth: visible ? 38 : 0
            Layout.preferredHeight: root.inputMinHeight
            ToolTip.visible: hovered
            ToolTip.text: "隐藏聊天"
            contentItem: Text {
                text: closeButton.text
                color: "#7b746d"
                font: closeButton.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Item {}
            onClicked: root.closeRequested()
        }
    }
}
```

- [ ] **Step 3: Build the QML module**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop
```

Expected: build succeeds and the generated QML module accepts `ChatComposer.qml`.

- [ ] **Step 4: Commit**

```bash
git add apps/desktop/CMakeLists.txt apps/desktop/qml/ChatComposer.qml
git commit -m "feat: 抽取聊天输入组件"
```

---

## Task 4: Refactor ChatWindow To Use The Composer

**Files:**

- Modify: `apps/desktop/qml/ChatWindow.qml`

- [ ] **Step 1: Update compact dimensions and expanded-state reporting**

In `apps/desktop/qml/ChatWindow.qml`, change:

```qml
    property int compactWidth: 420
    property int compactHeight: 128
```

to:

```qml
    property int compactWidth: 460
    property int compactMinHeight: 62
```

Replace:

```qml
    onVisibleChanged: App.DesktopShell.setChatWindowDockVisible(visible)
```

with:

```qml
    onVisibleChanged: {
        App.DesktopShell.setChatWindowDockVisible(visible)
        syncShellChatState()
    }

    onCompactModeChanged: syncShellChatState()

    function syncShellChatState() {
        App.DesktopShell.setChatWindowExpanded(visible && !compactMode)
    }
```

- [ ] **Step 2: Update mode switching functions**

Replace the existing `showExpanded()`, `showCompact()`, and `hideChatUi()` functions with:

```qml
    function showExpanded() {
        compactMode = false
        minimumWidth = 360
        minimumHeight = 420
        maximumHeight = 16777215
        width = Math.max(360, expandedWidth)
        height = Math.max(420, expandedHeight)
        syncShellChatState()
    }

    function showCompact() {
        if (!compactMode) {
            expandedWidth = width
            expandedHeight = height
        }
        conversationPanelOpen = false
        compactMode = true
        minimumWidth = compactWidth
        minimumHeight = compactMinHeight
        maximumHeight = compactComposer.implicitHeight + 24
        width = compactWidth
        height = compactComposer.implicitHeight + 24
        syncShellChatState()
        compactComposer.forceInputFocus()
    }

    function hideChatUi() {
        hide()
        syncShellChatState()
    }
```

After `input.forceActiveFocus()` in `open()`, replace it with:

```qml
        compactComposer.forceInputFocus()
```

- [ ] **Step 3: Remove the old submit helper**

Inside `ColumnLayout { id: chatLayout ... }`, delete this function completely:

```qml
        function submitInput() {
            if (App.ChatController.sending) {
                App.ChatController.cancelCurrentReply()
                return
            }

            const text = input.text.trim()
            if (text.length === 0) {
                return
            }
            input.text = ""
            App.ChatController.sendMessage(text)
        }
```

- [ ] **Step 4: Replace the old composer row**

Delete the final `RowLayout` containing `compactExpandButton`, `TextArea id: input`, `Button id: sendButton`, and `compactCloseButton`.

Insert this `Rectangle` in the same place:

```qml
        Rectangle {
            id: compactDragShell

            Layout.fillWidth: true
            Layout.preferredHeight: compactComposer.implicitHeight
            radius: chatWindow.compactMode ? 16 : 10
            color: chatWindow.compactMode ? "#fffdf8" : "transparent"
            border.color: chatWindow.compactMode ? "#d7cec3" : "transparent"
            border.width: chatWindow.compactMode ? 1 : 0

            MouseArea {
                anchors.fill: parent
                enabled: chatWindow.compactMode
                acceptedButtons: Qt.LeftButton
                propagateComposedEvents: true
                onPressed: function(mouse) {
                    if (mouse.y < 8 || mouse.x < 46 || mouse.x > width - 46) {
                        chatWindow.startSystemMove()
                    } else {
                        mouse.accepted = false
                    }
                }
            }

            ChatComposer {
                id: compactComposer

                anchors.fill: parent
                anchors.margins: chatWindow.compactMode ? 8 : 0
                compactMode: chatWindow.compactMode

                onSubmitRequested: function(text) {
                    clearText()
                    App.ChatController.sendMessage(text)
                }

                onCancelRequested: App.ChatController.cancelCurrentReply()
                onExpandRequested: App.ChatController.openWindow()
                onCloseRequested: chatWindow.hideChatUi()

                onImplicitHeightChanged: {
                    if (chatWindow.compactMode) {
                        chatWindow.height = implicitHeight + 24
                        chatWindow.maximumHeight = chatWindow.height
                    }
                }
            }
        }
```

- [ ] **Step 5: Remove old input references**

Run:

```bash
rg -n "\\binput\\b|compactExpandButton|sendButton|compactCloseButton|submitInput" apps/desktop/qml/ChatWindow.qml
```

Expected: no matches for `input`, `compactExpandButton`, `sendButton`, `compactCloseButton`, or `submitInput`.

- [ ] **Step 6: Build**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop
```

Expected: build succeeds.

- [ ] **Step 7: Commit**

```bash
git add apps/desktop/qml/ChatWindow.qml
git commit -m "feat: 统一聊天窗输入栏样式"
```

---

## Task 5: Comic Bubble Window

**Files:**

- Modify: `apps/desktop/qml/ChatBubbleWindow.qml`

- [ ] **Step 1: Add placement and display-state properties**

In `ChatBubbleWindow.qml`, replace the existing `width`, `height`, `screenMargin`, `x`, and `y` block with:

```qml
    readonly property int bubbleMargin: 12
    readonly property int contentHorizontalPadding: 28
    readonly property int contentVerticalPadding: 24
    readonly property int pointerExtent: 28
    readonly property int maxBubbleWidth: 360
    readonly property int minBubbleWidth: 210
    readonly property int bodyWidth: Math.max(minBubbleWidth,
            Math.min(maxBubbleWidth, messageMeasure.contentWidth + contentHorizontalPadding * 2))
    readonly property int bodyHeight: Math.max(82,
            Math.min(190, messageMeasure.contentHeight + contentVerticalPadding * 2))
    property string pointerPlacement: "bottomLeft"

    width: bodyWidth
    height: bodyHeight + pointerExtent
    onWidthChanged: updatePlacement()
    onHeightChanged: updatePlacement()

    function updatePlacement() {
        const nextPlacement = App.DesktopShell.placeChatBubble(width, height, bubbleMargin)
        x = nextPlacement.x || 0
        y = nextPlacement.y || 0
        pointerPlacement = nextPlacement.pointer || "bottomLeft"
        bubbleCanvas.requestPaint()
    }
```

Delete the old `clamp`, `clampedBubbleX`, and `clampedBubbleY` functions.

- [ ] **Step 2: Hide bubble while expanded chat is visible**

At the top of `syncAssistantBubble()`, after `const messages = App.ChatController.messages`, add:

```qml
        if (App.DesktopShell.chatWindowExpanded) {
            hideTimer.stop()
            visible = false
            return
        }
```

Add this connection block:

```qml
    Connections {
        target: App.DesktopShell

        function onChatWindowStateChanged() {
            bubbleWindow.syncAssistantBubble()
        }

        function onPetWindowGeometryChanged() {
            bubbleWindow.updatePlacement()
        }
    }
```

- [ ] **Step 3: Replace hide timing**

Replace `scheduleHide()` with:

```qml
    function scheduleHide() {
        hideTimer.interval = Math.max(2500, Math.min(10000, 2500 + Math.ceil(assistantText.length / 20) * 1000))
        if (!bubbleHover.hovered) {
            hideTimer.restart()
        }
    }
```

Inside the existing `HoverHandler`, add:

```qml
            onHoveredChanged: {
                if (hovered) {
                    hideTimer.stop()
                } else if (bubbleWindow.visible && bubbleWindow.assistantPending !== true) {
                    bubbleWindow.scheduleHide()
                }
            }
```

- [ ] **Step 4: Replace the rectangular frame with one Canvas path**

Replace `Rectangle { id: bubbleFrame ... }` with an `Item` root using this exact structure:

```qml
    Item {
        id: bubbleFrame

        anchors.fill: parent

        HoverHandler {
            id: bubbleHover
            onHoveredChanged: {
                if (hovered) {
                    hideTimer.stop()
                } else if (bubbleWindow.visible && bubbleWindow.assistantPending !== true) {
                    bubbleWindow.scheduleHide()
                }
            }
        }

        Canvas {
            id: bubbleCanvas

            anchors.fill: parent
            antialiasing: true
            onPaint: {
                const ctx = getContext("2d")
                const w = bubbleWindow.width
                const h = bubbleWindow.height
                const tail = bubbleWindow.pointerExtent
                const bodyTop = bubbleWindow.pointerPlacement.startsWith("top") ? tail : 2
                const bodyBottom = bubbleWindow.pointerPlacement.startsWith("bottom") ? h - tail : h - 2
                const radius = 22
                const left = 4
                const right = w - 4
                const tailLeft = bubbleWindow.pointerPlacement.endsWith("Left")
                const tailBaseX = tailLeft ? 52 : w - 52
                const tailTipX = tailLeft ? 28 : w - 28
                const tailTipY = bubbleWindow.pointerPlacement.startsWith("top") ? 4 : h - 4
                const tailBaseY = bubbleWindow.pointerPlacement.startsWith("top") ? bodyTop : bodyBottom

                ctx.clearRect(0, 0, w, h)
                ctx.beginPath()
                ctx.moveTo(left + radius, bodyTop)

                if (bubbleWindow.pointerPlacement === "topLeft") {
                    ctx.lineTo(tailBaseX, bodyTop)
                    ctx.lineTo(tailTipX, tailTipY)
                    ctx.lineTo(tailBaseX + 28, bodyTop)
                }
                ctx.lineTo(right - radius, bodyTop)
                if (bubbleWindow.pointerPlacement === "topRight") {
                    ctx.lineTo(tailBaseX + 28, bodyTop)
                    ctx.lineTo(tailTipX, tailTipY)
                    ctx.lineTo(tailBaseX, bodyTop)
                }
                ctx.quadraticCurveTo(right, bodyTop, right, bodyTop + radius)
                ctx.lineTo(right, bodyBottom - radius)
                ctx.quadraticCurveTo(right, bodyBottom, right - radius, bodyBottom)
                if (bubbleWindow.pointerPlacement === "bottomRight") {
                    ctx.lineTo(tailBaseX + 28, bodyBottom)
                    ctx.lineTo(tailTipX, tailTipY)
                    ctx.lineTo(tailBaseX, bodyBottom)
                }
                ctx.lineTo(left + radius, bodyBottom)
                if (bubbleWindow.pointerPlacement === "bottomLeft") {
                    ctx.lineTo(tailBaseX + 28, bodyBottom)
                    ctx.lineTo(tailTipX, tailTipY)
                    ctx.lineTo(tailBaseX, bodyBottom)
                }
                ctx.quadraticCurveTo(left, bodyBottom, left, bodyBottom - radius)
                ctx.lineTo(left, bodyTop + radius)
                ctx.quadraticCurveTo(left, bodyTop, left + radius, bodyTop)
                ctx.closePath()

                ctx.fillStyle = "#fffdf8"
                ctx.fill()
                ctx.lineWidth = 3
                ctx.strokeStyle = "#6f6a63"
                ctx.stroke()
            }

            Connections {
                target: bubbleWindow
                function onPointerPlacementChanged() { bubbleCanvas.requestPaint() }
            }
        }
```

Keep `messageMeasure`, the action row, and text content inside this new `Item` after `Canvas`.

- [ ] **Step 5: Convert bubble controls to hover-only glyphs**

Replace the existing `bubbleActions` `RowLayout` with:

```qml
        Row {
            id: bubbleActions

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: bubbleWindow.pointerPlacement.startsWith("top") ? bubbleWindow.pointerExtent + 16 : 18
            anchors.rightMargin: 22
            spacing: 10
            opacity: bubbleHover.hovered ? 1 : 0
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation { duration: 100 }
            }

            ToolButton {
                id: expandButton

                text: "↗"
                font.pixelSize: 18
                width: 20
                height: 20
                ToolTip.visible: hovered
                ToolTip.text: "展开聊天"
                contentItem: Text {
                    text: expandButton.text
                    color: "#8d8780"
                    font: expandButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
                onClicked: App.ChatController.openWindow()
            }

            ToolButton {
                id: closeBubbleButton

                text: "×"
                font.pixelSize: 18
                width: 20
                height: 20
                ToolTip.visible: hovered
                ToolTip.text: "隐藏气泡"
                contentItem: Text {
                    text: closeBubbleButton.text
                    color: "#8d8780"
                    font: closeBubbleButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
                onClicked: bubbleWindow.hideCurrentBubble()
            }
        }
```

- [ ] **Step 6: Align text to the comic body**

Replace `ScrollView` anchors with:

```qml
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: bubbleWindow.contentHorizontalPadding
            anchors.rightMargin: bubbleWindow.contentHorizontalPadding
            anchors.topMargin: bubbleWindow.pointerPlacement.startsWith("top")
                    ? bubbleWindow.pointerExtent + bubbleWindow.contentVerticalPadding
                    : bubbleWindow.contentVerticalPadding
            anchors.bottomMargin: bubbleWindow.pointerPlacement.startsWith("bottom")
                    ? bubbleWindow.pointerExtent + bubbleWindow.contentVerticalPadding
                    : bubbleWindow.contentVerticalPadding
```

- [ ] **Step 7: Build**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop
```

Expected: build succeeds.

- [ ] **Step 8: Commit**

```bash
git add apps/desktop/qml/ChatBubbleWindow.qml
git commit -m "feat: 优化桌宠回复漫画气泡"
```

---

## Task 6: Verification And Visual Pass

**Files:**

- Modify: `README.md` only if the implementation changes user-facing run instructions.

- [ ] **Step 1: Run targeted tests**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop ChatBubblePlacementSmoke ChatControllerSmoke ChatTextPacerSmoke
ctest --test-dir build -R "chat_bubble_placement_smoke|chat_controller_smoke|chat_text_pacer_smoke" --output-on-failure
```

Expected: all listed tests pass.

- [ ] **Step 2: Run whitespace check**

Run:

```bash
git diff --check
```

Expected: no output.

- [ ] **Step 3: Launch the desktop app**

Run:

```bash
open build/apps/desktop/MilesEdgeworthDesktop.app
```

Expected: the app launches, the pet appears, and no QML load errors appear in the terminal/build output.

- [ ] **Step 4: Manual compact composer checks**

In the running app:

```text
1. Open chat from the pet context menu.
2. Click 收起.
3. Confirm compact width is about 420-480 px, not a wide bottom dock.
4. Confirm empty input is one line.
5. Type "这个推论哪里不成立？" and confirm the input remains one line.
6. Paste "我想让你从证据链的角度重新检查这段推理，尤其是结论成立之前是否缺少必要前提。如果有漏洞，请直接指出。" and confirm the composer grows, then caps height and scrolls internally.
7. Drag the compact shell by the side glyph area or top padding and confirm the window moves without moving the pet.
```

Expected: all seven checks match the design.

- [ ] **Step 5: Manual expanded checks**

In the running app:

```text
1. Click the compact ↗ glyph.
2. Confirm the expanded window opens with the same composer style.
3. Send a message while expanded.
4. Confirm the assistant reply streams only in the chat transcript.
5. Confirm no pet-head reply bubble is visible while expanded.
6. Click 收起 during or after the reply.
7. Confirm the bubble reappears only if the reply is still streaming or inside its post-completion stay window.
```

Expected: expanded mode uses the shared composer and suppresses the pet bubble.

- [ ] **Step 6: Manual bubble checks**

In the running app:

```text
1. Keep chat in compact mode.
2. Send a message and confirm the pet bubble streams the same assistant text as the transcript model.
3. Hover the bubble and confirm ↗ and × appear together in the same gray color.
4. Move the pet near each screen corner and send a short message:
   - top-left pet: bubble appears bottom-right with top-left pointer.
   - top-right pet: bubble appears bottom-left with top-right pointer.
   - bottom-left pet: bubble appears top-right with bottom-left pointer.
   - bottom-right pet: bubble appears top-left with bottom-right pointer.
5. Confirm the bubble outline is one clean comic shape with no inner seam.
6. Confirm the bubble stays inside the screen available geometry.
7. Stop hovering after completion and confirm the bubble auto-hides after roughly 2.5-10 seconds depending on text length.
```

Expected: all seven checks match the design.

- [ ] **Step 7: README check**

Run:

```bash
git diff -- README.md
```

Expected: no diff. If there is a pre-existing diff unrelated to this implementation, leave it untouched and mention it in the final handoff.

- [ ] **Step 8: Final commit**

If Task 6 produced only verification and no file changes:

```bash
git status --short
```

Expected: clean working tree. No commit.

If README or docs were legitimately updated:

```bash
git add README.md docs
git commit -m "docs: 更新迷你聊天验证说明"
```

---

## Self-Review Checklist

Spec coverage:

- Smaller compact input bar: Task 3, Task 4, Task 6.
- Compact empty/short one-line and long-input growth: Task 3, Task 4, Task 6.
- Send/stop inside composer lower-right: Task 3.
- Compact draggable and independent from pet: Task 4, Task 6.
- Expanded mode same composer style: Task 3, Task 4, Task 6.
- Expanded mode hides pet bubble: Task 2, Task 5, Task 6.
- Bubble follows latest assistant message without changing streaming: Task 5.
- Comic bubble continuous outline: Task 5.
- Hover-only same-color glyph controls: Task 5.
- Four-direction placement and screen clamp: Task 1, Task 2, Task 5, Task 6.
- Text-length-based stay and hover pause: Task 5, Task 6.
- Existing chat streaming tests pass: Task 0, Task 6.

Placeholder scan:

- Completed: the plan contains concrete file paths, commands, code blocks, and named APIs for each implementation task.

Type consistency:

- C++ helper type is `ChatBubblePlacementResult`.
- C++ helper function is `placeChatBubble(...)`.
- QML bridge method is `App.DesktopShell.placeChatBubble(...)`.
- QML expanded state property is `App.DesktopShell.chatWindowExpanded`.
- QML shared component is `ChatComposer`.
