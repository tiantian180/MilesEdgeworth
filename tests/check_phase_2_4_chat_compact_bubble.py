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
    pet_assets_qrc = read("apps/desktop/resources/pet_assets.qrc")
    main_cpp = read("apps/desktop/src/main.cpp")
    shell_h = read("apps/desktop/src/DesktopShellController.h")
    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    bubble_qml = read("apps/desktop/qml/ChatBubbleWindow.qml")
    placement_cpp = read("apps/desktop/src/chat/ChatBubblePlacement.cpp")
    placement_smoke = read("apps/desktop/tests/chat_bubble_placement_smoke.cpp")

    require(
        "check_phase_2_4_chat_compact_bubble" in root_cmake,
        "root CMake must register the compact chat bubble contract check",
    )
    require(
        "qml/ChatBubbleWindow.qml" in desktop_cmake,
        "desktop QML module must include ChatBubbleWindow.qml",
    )

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
        "Q_PROPERTY(int petScreenAvailableX READ petScreenAvailableX NOTIFY petWindowGeometryChanged)",
        "Q_PROPERTY(int petScreenAvailableY READ petScreenAvailableY NOTIFY petWindowGeometryChanged)",
        "Q_PROPERTY(int petScreenAvailableWidth READ petScreenAvailableWidth NOTIFY petWindowGeometryChanged)",
        "Q_PROPERTY(int petScreenAvailableHeight READ petScreenAvailableHeight NOTIFY petWindowGeometryChanged)",
        "Q_PROPERTY(bool chatWindowExpanded READ chatWindowExpanded NOTIFY chatWindowStateChanged)",
        "Q_INVOKABLE QVariantMap placeChatBubble(int bubbleWidth, int bubbleHeight, int margin) const;",
        "void petWindowGeometryChanged();",
        "void chatWindowStateChanged();",
    ]:
        require(token in shell_h, f"DesktopShellController.h missing {token}")
    for token in [
        "QWindow::xChanged",
        "QWindow::yChanged",
        "QWindow::widthChanged",
        "QWindow::heightChanged",
        "QWindow::screenChanged",
        "availableGeometry()",
        "emit petWindowGeometryChanged();",
        "emit chatWindowStateChanged();",
        "const ChatBubblePlacementResult placement = ::placeChatBubble(",
        "result.insert(QStringLiteral(\"pointer\"), placement.pointer);",
    ]:
        require(token in shell_cpp, f"DesktopShellController.cpp missing {token}")

    for token in [
        "pet.right() + 1 + safeMargin",
        "pet.bottom() + 1 + safeMargin",
        "available.right() - bubbleWidth - safeMargin + 1",
        "available.bottom() - bubbleHeight - safeMargin + 1",
        "QStringLiteral(\"topLeft\")",
        "QStringLiteral(\"topRight\")",
        "QStringLiteral(\"bottomLeft\")",
        "QStringLiteral(\"bottomRight\")",
    ]:
        require(token in placement_cpp, f"ChatBubblePlacement.cpp missing {token}")
    for token in [
        "top-left pet should keep an exact horizontal margin",
        "top-right pet should keep an exact horizontal margin",
        "bottom-left pet should keep an exact vertical margin",
        "bottom-right pet should keep an exact vertical margin",
        "normal bubble should clamp to the available right margin",
        "normal bubble should clamp to the available bottom margin with a shifted screen",
    ]:
        require(token in placement_smoke, f"chat_bubble_placement_smoke.cpp missing {token}")

    for token in [
        "property bool compactMode",
        "property int compactWidth: 460",
        "property int compactMinHeight: 62",
        "function syncShellChatState()",
        "function showCompact()",
        "function showExpanded()",
        "function hideChatUi()",
        "ChatComposer",
        "id: compactComposer",
        "compactMode: chatWindow.compactMode",
        "height = compactComposer.implicitHeight + 24",
        "App.DesktopShell.setChatWindowExpanded(visible && !compactMode)",
        "visible: !chatWindow.compactMode",
        "enabled: chatWindow.compactMode",
        "text: \"收起\"",
        "onExpandRequested: App.ChatController.openWindow()",
        "onCloseRequested: chatWindow.hideChatUi()",
        "App.ChatController.sendMessage(text)",
    ]:
        require(token in chat_qml, f"ChatWindow.qml missing {token}")

    for token in [
        "ApplicationWindow",
        "App.ChatController.messages",
        "role === \"assistant\"",
        "pending === true",
        "function syncAssistantBubble()",
        "function updatePlacement()",
        "App.DesktopShell.placeChatBubble(width, height, bubbleMargin)",
        "property string pointerPlacement",
        "App.DesktopShell.chatWindowExpanded",
        "function hideForExpandedChat()",
        "bubbleWindow.hideForExpandedChat()",
        "function onSendingChanged()",
        "bubbleWindow.suppressNextAssistantBubble = false",
        "function onChatWindowStateChanged()",
        "function onPetWindowGeometryChanged()",
        "const interval = Math.max(2500, Math.min(10000, 2500 + Math.ceil(assistantText.length / 20) * 1000))",
        "function startHideTimer(interval)",
        "hideDeadlineMs",
        "remainingHideMs",
        "Canvas",
        "ctx.beginPath()",
        "ctx.quadraticCurveTo",
        "ctx.closePath()",
        "ctx.stroke()",
        "Text {",
        "id: messageMeasure",
        "ScrollView",
        "HoverHandler",
        "App.ChatController.openWindow()",
        "bubbleDismissed = true",
        "opacity: bubbleHover.hovered ? 1 : 0",
        "visible: opacity > 0",
        "text: \"↗\"",
        "text: \"×\"",
        "color: \"#8d8780\"",
    ]:
        require(token in bubble_qml, f"ChatBubbleWindow.qml missing {token}")

    print("phase 2.4 compact chat bubble contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
