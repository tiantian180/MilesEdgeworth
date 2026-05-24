# Chat Compact Mode And Pet Bubble Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a compact chat mode that keeps only the input composer visible and mirrors Miles's streamed reply into a pet-head speech bubble.

**Architecture:** Keep `ChatController` as the single source of truth for message text and streaming pace. `ChatWindow.qml` becomes a two-mode view over the existing message/composer model, while a new `ChatBubbleWindow.qml` mirrors the latest assistant message from `App.ChatController.messages` and positions itself above the native pet window through small geometry properties exposed by `DesktopShellController`. No SSE, pacer, segment queue, or cleanFinish behavior changes.

**Tech Stack:** Qt 6 / QML `ApplicationWindow`, Qt Quick Controls Basic style, C++17 `DesktopShellController`, Python static contract check, CMake / CTest.

**Implementation note:** Review feedback tightened the final implementation beyond the initial snippets below: bubble positioning clamps against the pet window's current screen `availableGeometry()`, in-flight bubble suppression is cleared on `sendingChanged`, and the hover-close contract checks real opacity/enabled behavior instead of a comment token.

---

## Scope Check

This is one UI feature with three tightly coupled surfaces:

- full chat window: add a collapse entry point;
- compact chat window: hide transcript and keep the composer;
- pet speech bubble: show the latest assistant reply at the same streamed pace.

The feature intentionally does not change `ChatController::sendMessage`, `ChatTextPacer`, SSE parsing, conversation persistence, or Phase 2.4 animation gating. The bubble reads the already-paced text from `App.ChatController.messages`, so it uses the same output rhythm as the expanded transcript.

## File Structure

- Create: `tests/check_phase_2_4_chat_compact_bubble.py`
  - Static contract check for compact mode, bubble window registration, pet geometry exposure, and root CMake registration.
- Modify: `CMakeLists.txt`
  - Register the new Python check with CTest.
- Modify: `apps/desktop/CMakeLists.txt`
  - Register `qml/ChatBubbleWindow.qml` in the `MilesEdgeworth` QML module.
- Modify: `apps/desktop/src/DesktopShellController.h`
  - Expose pet window geometry as QML-readable properties.
- Modify: `apps/desktop/src/DesktopShellController.cpp`
  - Emit pet geometry changes when the native pet window moves or resizes.
- Modify: `apps/desktop/qml/ChatWindow.qml`
  - Add `compactMode`, collapse/expand/close actions, and reuse the current input composer.
- Create: `apps/desktop/qml/ChatBubbleWindow.qml`
  - Mirror latest assistant text, show while streaming, auto-hide after completion based on text length, and provide expand/hover-close buttons.
- Modify: `apps/desktop/src/main.cpp`
  - Load `ChatBubbleWindow` as another QML root after the singletons are installed.

## Task 1: Contract Check

**Files:**

- Create: `tests/check_phase_2_4_chat_compact_bubble.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add the failing contract check**

Create `tests/check_phase_2_4_chat_compact_bubble.py` with this exact content:

```python
#!/usr/bin/env python3
"""Check compact chat window and pet speech bubble contracts."""

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
    root_cmake = read("CMakeLists.txt")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    main_cpp = read("apps/desktop/src/main.cpp")
    shell_h = read("apps/desktop/src/DesktopShellController.h")
    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    bubble_qml = read("apps/desktop/qml/ChatBubbleWindow.qml")

    require(
        "check_phase_2_4_chat_compact_bubble" in root_cmake,
        "root CMake must register the compact chat bubble contract check",
    )
    require(
        "qml/ChatBubbleWindow.qml" in desktop_cmake,
        "desktop QML module must include ChatBubbleWindow.qml",
    )
    require(
        'loadFromModule("MilesEdgeworth", "ChatBubbleWindow")' in main_cpp,
        "main must load ChatBubbleWindow from the MilesEdgeworth QML module",
    )
    require(
        "rootObjects().size() < 3" in main_cpp,
        "main must expect ChatWindow, SettingsWindow, and ChatBubbleWindow roots",
    )

    for token in [
        "Q_PROPERTY(int petWindowX READ petWindowX NOTIFY petWindowGeometryChanged)",
        "Q_PROPERTY(int petWindowY READ petWindowY NOTIFY petWindowGeometryChanged)",
        "Q_PROPERTY(int petWindowWidth READ petWindowWidth NOTIFY petWindowGeometryChanged)",
        "Q_PROPERTY(int petWindowHeight READ petWindowHeight NOTIFY petWindowGeometryChanged)",
        "void petWindowGeometryChanged();",
    ]:
        require(token in shell_h, f"DesktopShellController.h missing {token}")
    for token in [
        "QWindow::xChanged",
        "QWindow::yChanged",
        "QWindow::widthChanged",
        "QWindow::heightChanged",
        "emit petWindowGeometryChanged();",
    ]:
        require(token in shell_cpp, f"DesktopShellController.cpp missing {token}")

    for token in [
        "property bool compactMode",
        "function showCompact()",
        "function showExpanded()",
        "function hideChatUi()",
        "height = compactHeight",
        "visible: !chatWindow.compactMode",
        "visible: chatWindow.compactMode",
        "text: \"收起\"",
        "text: \"展开\"",
        "text: \"关闭\"",
        "chatLayout.submitInput()",
        "App.ChatController.sendMessage(text)",
    ]:
        require(token in chat_qml, f"ChatWindow.qml missing {token}")

    for token in [
        "ApplicationWindow",
        "App.ChatController.messages",
        "role === \"assistant\"",
        "pending === true",
        "function syncAssistantBubble()",
        "hideTimer.interval = Math.max(4000, Math.min(12000, 3000 + assistantText.length * 80))",
        "App.DesktopShell.petWindowX",
        "App.DesktopShell.petWindowY",
        "App.DesktopShell.petWindowWidth",
        "HoverHandler",
        "App.ChatController.openWindow()",
        "bubbleDismissed = true",
        "visible: bubbleHover.hovered",
    ]:
        require(token in bubble_qml, f"ChatBubbleWindow.qml missing {token}")

    print("phase 2.4 compact chat bubble contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Register the contract check in root CMake**

In `CMakeLists.txt`, immediately after the existing `check_phase_2_4_phased_animation` test block, insert:

```cmake
        # Compact chat bubble check: keeps the compact composer and pet-head
        # assistant bubble as a UI mirror over ChatController.messages.
        add_test(
            NAME check_phase_2_4_chat_compact_bubble
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_2_4_chat_compact_bubble.py
        )
```

- [ ] **Step 3: Run the contract check to verify it fails**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
```

Expected: FAIL with `missing file: apps/desktop/qml/ChatBubbleWindow.qml`.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt tests/check_phase_2_4_chat_compact_bubble.py
git commit -m "test: 增加迷你聊天气泡契约检查"
```

## Task 2: Pet Window Geometry For Bubble Positioning

**Files:**

- Modify: `apps/desktop/src/DesktopShellController.h`
- Modify: `apps/desktop/src/DesktopShellController.cpp`

- [ ] **Step 1: Add pet geometry properties to the shell controller header**

In `apps/desktop/src/DesktopShellController.h`, extend the existing `Q_PROPERTY` block with:

```cpp
    Q_PROPERTY(int petWindowX READ petWindowX NOTIFY petWindowGeometryChanged)
    Q_PROPERTY(int petWindowY READ petWindowY NOTIFY petWindowGeometryChanged)
    Q_PROPERTY(int petWindowWidth READ petWindowWidth NOTIFY petWindowGeometryChanged)
    Q_PROPERTY(int petWindowHeight READ petWindowHeight NOTIFY petWindowGeometryChanged)
```

In the public getter section, after `int screenCount() const;`, add:

```cpp
    int petWindowX() const;
    int petWindowY() const;
    int petWindowWidth() const;
    int petWindowHeight() const;
```

In the signals section, after `void screenCountChanged();`, add:

```cpp
    void petWindowGeometryChanged();
```

- [ ] **Step 2: Implement pet geometry getters**

In `apps/desktop/src/DesktopShellController.cpp`, after `int DesktopShellController::screenCount() const`, add:

```cpp
int DesktopShellController::petWindowX() const
{
    return m_petWindow != nullptr ? m_petWindow->x() : 0;
}

int DesktopShellController::petWindowY() const
{
    return m_petWindow != nullptr ? m_petWindow->y() : 0;
}

int DesktopShellController::petWindowWidth() const
{
    return m_petWindow != nullptr ? m_petWindow->width() : 0;
}

int DesktopShellController::petWindowHeight() const
{
    return m_petWindow != nullptr ? m_petWindow->height() : 0;
}
```

- [ ] **Step 3: Emit geometry changes from `setPetWindow`**

In `DesktopShellController::setPetWindow`, replace the method body with:

```cpp
void DesktopShellController::setPetWindow(QWindow *window)
{
    if (m_petWindow == window) {
        return;
    }

    if (m_petWindow != nullptr) {
        disconnect(m_petWindow, nullptr, this, nullptr);
    }

    m_petWindow = window;

    if (m_petWindow != nullptr) {
        auto notifyGeometryChanged = [this]() {
            emit petWindowGeometryChanged();
        };
        connect(m_petWindow, &QWindow::xChanged, this, notifyGeometryChanged);
        connect(m_petWindow, &QWindow::yChanged, this, notifyGeometryChanged);
        connect(m_petWindow, &QWindow::widthChanged, this, notifyGeometryChanged);
        connect(m_petWindow, &QWindow::heightChanged, this, notifyGeometryChanged);
    }

#ifdef Q_OS_MACOS
    // 基础行为只负责“像桌宠窗口”：不因失焦隐藏、透明、禁用普通窗口动画。
    // 是否置顶单独由 applyCurrentLayerMode() 决定，方便菜单动态切换。
    applyMacPetWindowBaseBehavior(m_petWindow);
#endif

    applyCurrentLayerMode();
    emit petWindowGeometryChanged();
}
```

- [ ] **Step 4: Emit geometry changes after shell-controlled moves**

In `DesktopShellController::placePetWindowForStartup`, after `m_petWindow->setPosition(startupPosition.toPoint());`, add:

```cpp
    emit petWindowGeometryChanged();
```

In `DesktopShellController::movePetWindowTo`, after `m_petWindow->setPosition(clampedPosition.toPoint());`, add:

```cpp
    emit petWindowGeometryChanged();
```

- [ ] **Step 5: Build the desktop target**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop
```

Expected: build succeeds.

- [ ] **Step 6: Run the contract check to verify the remaining failure is only the missing bubble/UI work**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
```

Expected: FAIL with `missing file: apps/desktop/qml/ChatBubbleWindow.qml`.

- [ ] **Step 7: Commit**

```bash
git add apps/desktop/src/DesktopShellController.h apps/desktop/src/DesktopShellController.cpp
git commit -m "feat: 暴露桌宠窗口几何信息"
```

## Task 3: Compact Mode In ChatWindow

**Files:**

- Modify: `apps/desktop/qml/ChatWindow.qml`

- [ ] **Step 1: Add compact-mode state and mode functions**

In `apps/desktop/qml/ChatWindow.qml`, inside the root `ApplicationWindow`, after:

```qml
    property bool conversationPanelOpen: false
    property string pendingDeleteConversationId: ""
    property string pendingDeleteConversationTitle: ""
```

add:

```qml
    property bool compactMode: false
    property int expandedWidth: 420
    property int expandedHeight: 560
    property int compactWidth: 420
    property int compactHeight: 128
```

Replace the existing `function open()` with:

```qml
    function open() {
        showExpanded()
        App.ChatController.loadConversations()
        show()
        raise()
        requestActivate()
        input.forceActiveFocus()
    }

    function showExpanded() {
        compactMode = false
        minimumWidth = 360
        minimumHeight = 420
        width = Math.max(360, expandedWidth)
        height = Math.max(420, expandedHeight)
    }

    function showCompact() {
        if (!compactMode) {
            expandedWidth = width
            expandedHeight = height
        }
        conversationPanelOpen = false
        compactMode = true
        minimumWidth = 360
        minimumHeight = compactHeight
        width = Math.max(360, Math.min(width, compactWidth))
        height = compactHeight
        input.forceActiveFocus()
    }

    function hideChatUi() {
        hide()
    }
```

- [ ] **Step 2: Hide the expanded header in compact mode and add the collapse button**

In the first `RowLayout` under `ColumnLayout { id: chatLayout ... }`, add these layout properties immediately after `spacing: 8`:

```qml
            visible: !chatWindow.compactMode
            Layout.preferredHeight: chatWindow.compactMode ? 0 : 36
```

In that same header row, after the existing `reconnectButton` block, add:

```qml
            Button {
                id: collapseButton

                text: "收起"
                font.pixelSize: 13
                Layout.preferredWidth: 76
                Layout.preferredHeight: 36
                contentItem: Text {
                    text: collapseButton.text
                    color: collapseButton.enabled ? "#5a4031" : "#9a9086"
                    font: collapseButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: collapseButton.down ? "#e1d8ce" : "#f3ede5"
                    border.color: "#bfae9e"
                }
                onClicked: chatWindow.showCompact()
            }
```

- [ ] **Step 3: Hide the transcript/conversation body in compact mode**

In the large body `RowLayout` that contains the conversation panel and transcript, add these properties immediately after its `spacing` line:

```qml
            visible: !chatWindow.compactMode
            Layout.fillHeight: !chatWindow.compactMode
            Layout.preferredHeight: chatWindow.compactMode ? 0 : -1
```

- [ ] **Step 4: Add compact controls to the composer row**

In the bottom composer `RowLayout`, immediately after `spacing: 8`, add:

```qml
            Layout.preferredHeight: chatWindow.compactMode ? 56 : 72
```

At the start of this composer row, before the existing `TextArea { id: input ... }`, add:

```qml
            Button {
                id: compactExpandButton

                visible: chatWindow.compactMode
                text: "展开"
                font.pixelSize: 13
                Layout.preferredWidth: visible ? 64 : 0
                Layout.preferredHeight: chatWindow.compactMode ? 52 : 72
                contentItem: Text {
                    text: compactExpandButton.text
                    color: "#5a4031"
                    font: compactExpandButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: compactExpandButton.down ? "#e1d8ce" : "#f3ede5"
                    border.color: "#bfae9e"
                }
                onClicked: chatWindow.open()
            }
```

In the existing `TextArea { id: input ... }`, replace:

```qml
                Layout.preferredHeight: 72
```

with:

```qml
                Layout.preferredHeight: chatWindow.compactMode ? 52 : 72
```

In the existing `Button { id: sendButton ... }`, replace:

```qml
                Layout.preferredHeight: 72
```

with:

```qml
                Layout.preferredHeight: chatWindow.compactMode ? 52 : 72
```

After the existing `sendButton` block, add:

```qml
            Button {
                id: compactCloseButton

                visible: chatWindow.compactMode
                text: "关闭"
                font.pixelSize: 13
                Layout.preferredWidth: visible ? 64 : 0
                Layout.preferredHeight: chatWindow.compactMode ? 52 : 72
                contentItem: Text {
                    text: compactCloseButton.text
                    color: "#5a4031"
                    font: compactCloseButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: compactCloseButton.down ? "#e1d8ce" : "#f3ede5"
                    border.color: "#bfae9e"
                }
                onClicked: chatWindow.hideChatUi()
            }
```

- [ ] **Step 5: Keep the existing submit path intact**

Confirm the composer still contains this existing key handler:

```qml
                Keys.onPressed: function(event) {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && (event.modifiers & Qt.ShiftModifier) === 0) {
                        event.accepted = true
                        chatLayout.submitInput()
                    }
                }
```

Confirm `sendButton` still ends with:

```qml
                onClicked: chatLayout.submitInput()
```

These two calls keep the static Phase 2.0 contract and the current sending behavior unchanged.

- [ ] **Step 6: Run the contract check to verify the remaining failure is only the missing bubble**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
```

Expected: FAIL with `missing file: apps/desktop/qml/ChatBubbleWindow.qml`.

- [ ] **Step 7: Commit**

```bash
git add apps/desktop/qml/ChatWindow.qml
git commit -m "feat: 增加聊天窗迷你输入态"
```

## Task 4: Pet Speech Bubble Window

**Files:**

- Create: `apps/desktop/qml/ChatBubbleWindow.qml`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/src/main.cpp`

- [ ] **Step 1: Add the bubble QML file**

Create `apps/desktop/qml/ChatBubbleWindow.qml` with this exact content:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MilesEdgeworth as App

ApplicationWindow {
    id: bubbleWindow

    width: 340
    height: Math.min(180, Math.max(78, messageMeasure.contentHeight + 58))
    visible: false
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    title: "Miles Reply"

    property string assistantText: ""
    property bool assistantPending: false
    property bool bubbleDismissed: false

    x: Math.round(App.DesktopShell.petWindowX
                  + App.DesktopShell.petWindowWidth / 2
                  - width / 2)
    y: Math.round(App.DesktopShell.petWindowY - height - 10)

    function syncAssistantBubble() {
        const messages = App.ChatController.messages
        let found = false
        let nextText = ""
        let nextPending = false

        for (let i = messages.length - 1; i >= 0; --i) {
            const message = messages[i]
            if (message.role === "assistant") {
                nextText = message.text || ""
                nextPending = message.pending === true
                found = true
                break
            }
        }

        if (!found || nextText.length === 0) {
            assistantText = ""
            assistantPending = false
            hideTimer.stop()
            visible = false
            return
        }

        const previousText = assistantText
        assistantText = nextText
        assistantPending = nextPending

        if (nextText.length > previousText.length || nextText !== previousText) {
            bubbleDismissed = false
        }

        if (!bubbleDismissed) {
            visible = true
            raise()
        }

        if (assistantPending === true) {
            hideTimer.stop()
            return
        }

        scheduleHide()
    }

    function scheduleHide() {
        hideTimer.interval = Math.max(4000, Math.min(12000, 3000 + assistantText.length * 80))
        hideTimer.restart()
    }

    Connections {
        target: App.ChatController

        function onMessagesChanged() {
            bubbleWindow.syncAssistantBubble()
        }

        function onOpenWindowRequested() {
            bubbleWindow.hideTimer.stop()
        }
    }

    Component.onCompleted: syncAssistantBubble()

    Timer {
        id: hideTimer

        repeat: false
        onTriggered: bubbleWindow.visible = false
    }

    Rectangle {
        id: bubbleFrame

        anchors.fill: parent
        radius: 8
        color: "#fffdf8"
        border.color: "#bfae9e"
        border.width: 1

        HoverHandler {
            id: bubbleHover
        }

        Text {
            id: messageMeasure

            visible: false
            width: bubbleText.width
            text: bubbleWindow.assistantText
            font.pixelSize: 14
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }

        RowLayout {
            id: bubbleActions

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 6
            anchors.rightMargin: 6
            spacing: 4

            ToolButton {
                id: expandButton

                text: "展开"
                font.pixelSize: 12
                Layout.preferredWidth: 44
                Layout.preferredHeight: 26
                contentItem: Text {
                    text: expandButton.text
                    color: "#5a4031"
                    font: expandButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: expandButton.down ? "#e1d8ce" : "#f3ede5"
                    border.color: "#d8d1c8"
                }
                onClicked: App.ChatController.openWindow()
            }

            ToolButton {
                id: closeBubbleButton

                visible: bubbleHover.hovered
                text: "关闭"
                font.pixelSize: 12
                Layout.preferredWidth: visible ? 44 : 0
                Layout.preferredHeight: 26
                contentItem: Text {
                    text: closeBubbleButton.text
                    color: "#5a4031"
                    font: closeBubbleButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: closeBubbleButton.down ? "#e1d8ce" : "#f3ede5"
                    border.color: "#d8d1c8"
                }
                onClicked: {
                    bubbleWindow.bubbleDismissed = true
                    bubbleWindow.hideTimer.stop()
                    bubbleWindow.visible = false
                }
            }
        }

        ScrollView {
            id: bubbleScroll

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 36
            anchors.bottomMargin: 12
            clip: true

            TextArea {
                id: bubbleText

                text: bubbleWindow.assistantText
                readOnly: true
                selectByMouse: true
                wrapMode: TextArea.Wrap
                textFormat: TextEdit.PlainText
                color: "#26201b"
                selectedTextColor: "#26201b"
                selectionColor: "#b9d0f2"
                font.pixelSize: 14
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                background: Item {}

                onTextChanged: cursorPosition = text.length
            }
        }
    }
}
```

- [ ] **Step 2: Register the bubble in the QML module**

In `apps/desktop/CMakeLists.txt`, in the `qt_add_qml_module(... QML_FILES ...)` list, add:

```cmake
        qml/ChatBubbleWindow.qml
```

The QML file list should contain:

```cmake
    QML_FILES
        qml/ChatBubbleWindow.qml
        qml/ChatWindow.qml
        qml/PetWindow.qml
        qml/SettingsWindow.qml
```

- [ ] **Step 3: Load the bubble window root**

In `apps/desktop/src/main.cpp`, replace:

```cpp
    chatEngine.loadFromModule("MilesEdgeworth", "ChatWindow");
    chatEngine.loadFromModule("MilesEdgeworth", "SettingsWindow");
    if (chatEngine.rootObjects().size() < 2) {
        return 1;
    }
```

with:

```cpp
    chatEngine.loadFromModule("MilesEdgeworth", "ChatWindow");
    chatEngine.loadFromModule("MilesEdgeworth", "SettingsWindow");
    chatEngine.loadFromModule("MilesEdgeworth", "ChatBubbleWindow");
    if (chatEngine.rootObjects().size() < 3) {
        return 1;
    }
```

- [ ] **Step 4: Run the contract check**

Run:

```bash
python3 tests/check_phase_2_4_chat_compact_bubble.py
```

Expected: PASS and prints `phase 2.4 compact chat bubble contract ok`.

- [ ] **Step 5: Build the desktop target**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop
```

Expected: build succeeds.

- [ ] **Step 6: Commit**

```bash
git add apps/desktop/CMakeLists.txt apps/desktop/qml/ChatBubbleWindow.qml apps/desktop/src/main.cpp
git commit -m "feat: 增加桌宠头顶回复气泡"
```

## Task 5: Verification Pass

**Files:**

- No source edits expected.

- [ ] **Step 1: Run the new contract through CTest**

Run:

```bash
ctest --test-dir build --output-on-failure -R 'check_phase_2_4_chat_compact_bubble'
```

Expected: PASS.

- [ ] **Step 2: Run the existing chat and Phase 2.4 checks**

Run:

```bash
ctest --test-dir build --output-on-failure -R 'check_phase_2_0_ai_chat_mvp|check_phase_2_3_2_session_persona|check_phase_2_4_phased_animation'
```

Expected: PASS.

- [ ] **Step 3: Run focused desktop smoke targets**

Run:

```bash
cmake --build build --target ChatControllerSmoke ChatTextPacerSmoke
ctest --test-dir build --output-on-failure -R 'chat_controller_smoke|chat_text_pacer_smoke'
```

Expected: PASS.

- [ ] **Step 4: Run a manual UI smoke**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop
open build/apps/desktop/MilesEdgeworthDesktop.app
```

Manual checks:

- Open chat from the pet context menu.
- Confirm the expanded chat window still shows conversation list, transcript, settings, reconnect, input, and send/stop.
- Click `收起`; confirm the window becomes a compact composer with `展开`, input, send/stop, and `关闭`.
- Send a message from compact mode; confirm the pet bubble shows only Miles's assistant reply and grows at the same pace as the transcript when expanded.
- Click the bubble `展开` button; confirm the full chat window opens in expanded mode and shows the complete conversation.
- Hover the bubble; confirm `关闭` appears. Click it; confirm the bubble hides while the reply continues and the chat record remains intact.
- In compact mode, click `关闭`; confirm only the chat UI hides and a continuing reply keeps updating the bubble.
- Let a reply finish; confirm the bubble remains visible for a few seconds and then hides.

- [ ] **Step 5: Review diff hygiene**

Run:

```bash
git diff --stat
git diff -- apps/desktop/qml/ChatWindow.qml apps/desktop/qml/ChatBubbleWindow.qml apps/desktop/src/DesktopShellController.h apps/desktop/src/DesktopShellController.cpp apps/desktop/src/main.cpp apps/desktop/CMakeLists.txt CMakeLists.txt tests/check_phase_2_4_chat_compact_bubble.py
```

Expected:

- No changes to `apps/desktop/src/chat/ChatController.cpp`.
- No changes to `apps/desktop/src/chat/ChatTextPacer.cpp`.
- No changes to Go sidecar provider/API code.
- UI changes are limited to compact display mode, bubble display, QML registration, and pet geometry exposure.

- [ ] **Step 6: Commit verification-only fixes if manual smoke found a UI issue**

If the manual smoke required a small UI-only adjustment, commit it with:

```bash
git add apps/desktop/qml/ChatWindow.qml apps/desktop/qml/ChatBubbleWindow.qml apps/desktop/src/DesktopShellController.h apps/desktop/src/DesktopShellController.cpp apps/desktop/src/main.cpp apps/desktop/CMakeLists.txt CMakeLists.txt tests/check_phase_2_4_chat_compact_bubble.py
git commit -m "fix: 修正迷你聊天气泡体验"
```

If no adjustment was needed, skip this commit step.

## Self-Review Notes

- Spec coverage: compact input mode is covered by Task 3; streamed assistant-only bubble, expand button, hover close, and length-based hide are covered by Task 4; geometry positioning is covered by Task 2; verification is covered by Task 5.
- Placeholder scan: the plan contains concrete file paths, code snippets, commands, expected results, and no open-ended implementation slots.
- Type consistency: QML uses `compactMode`, `showCompact`, `showExpanded`, `hideChatUi`, `assistantText`, `assistantPending`, and `bubbleDismissed` consistently; C++ uses `petWindowX`, `petWindowY`, `petWindowWidth`, `petWindowHeight`, and `petWindowGeometryChanged` consistently.
