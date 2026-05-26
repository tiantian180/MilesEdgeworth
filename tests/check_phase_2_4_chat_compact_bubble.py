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
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    visible_bounds_h = read("apps/desktop/src/pet/surface/PetVisibleBounds.h")
    visible_bounds_cpp = read("apps/desktop/src/pet/surface/PetVisibleBounds.cpp")
    visible_bounds_smoke = read("apps/desktop/tests/pet_visible_bounds_smoke.cpp")
    composer_qml = read("apps/desktop/qml/ChatComposer.qml")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    icon_button_qml = read("apps/desktop/qml/MilesIconButton.qml")
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

    for token in [
        "implicitWidth: 34",
        "implicitHeight: 34",
    ]:
        require(token in icon_button_qml, f"MilesIconButton.qml missing {token}")

    for icon_name in [
        "menu",
        "settings",
        "refresh-ccw",
        "maximize-2",
        "minimize-2",
        "x",
        "arrow-up-white",
        "arrow-up-muted",
    ]:
        icon_svg = read(f"apps/desktop/resources/icons/{icon_name}.svg")
        require('fill="none"' in icon_svg, f"{icon_name}.svg must use fill=\"none\"")

    square_stop_svg = read("apps/desktop/resources/icons/square-stop.svg")
    require('fill="#5f554b"' in square_stop_svg, "square-stop.svg must use fill=\"#5f554b\"")

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
        "narrow bubble tailX should stay inside bubble",
        "pet with room above should place bubble above",
        "tailX should stay inside safe tail range",
    ]:
        require(token in placement_smoke, f"chat_bubble_placement_smoke.cpp missing {token}")
    require("result.insert(QStringLiteral(\"tailX\"), placement.tailX);" in shell_cpp,
            "DesktopShellController.placeChatBubble must expose tailX")

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

    for token in [
        "property bool compactMode",
        "property int compactWidth: 380",
        "property int compactMinHeight: 56",
        "function syncShellChatState()",
        "function showCompact()",
        "function showExpanded()",
        "function hideChatUi()",
        "ChatComposer",
        "id: compactComposer",
        "compactMode: chatWindow.compactMode",
        "height = compactComposer.implicitHeight",
        "maximumHeight = compactComposer.implicitHeight",
        "compactDragArea",
        "z: 0",
        "z: 1",
        "startSystemMove()",
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
