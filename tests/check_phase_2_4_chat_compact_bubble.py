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
