#!/usr/bin/env python3
"""Check the Phase 2.0 AI chat MVP skeleton contract."""

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
    agent_cmake = read("apps/agent-core/CMakeLists.txt")
    go_mod = read("apps/agent-core/go.mod")
    server_go = read("apps/agent-core/internal/api/server.go")
    service_go = read("apps/agent-core/internal/chat/service/service.go")
    server_test = read("apps/agent-core/internal/api/server_test.go")
    parser_h = read("apps/desktop/src/chat/ChatStreamEvent.h")
    parser_cpp = read("apps/desktop/src/chat/ChatStreamEvent.cpp")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    composer_qml = read("apps/desktop/qml/ChatComposer.qml")
    main_cpp = read("apps/desktop/src/main.cpp")
    shell_h = read("apps/desktop/src/DesktopShellController.h")
    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    mac_behavior_h = read("apps/desktop/src/platform/MacPetWindowBehavior.h")
    mac_behavior_mm = read("apps/desktop/src/platform/MacPetWindowBehavior.mm")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    manifest = read("apps/desktop/resources/skins/miles-edgeworth/manifest.json")
    parser_smoke = read("apps/desktop/tests/chat_stream_event_parser_smoke.cpp")
    controller_smoke = read("apps/desktop/tests/chat_controller_smoke.cpp")
    phase_record = read("docs/v2/阶段记录/Phase 2.0 AI Chat MVP 骨架.md")
    index_doc = read("docs/v2/文档索引.md")

    require("add_subdirectory(apps/agent-core)" in root_cmake, "root CMake must include apps/agent-core")
    require("check_phase_2_0_ai_chat_mvp" in root_cmake, "root CMake must register Phase 2.0 check")

    for token in ["Network", "QuickControls2", "ChatController.cpp", "ChatStreamEvent.cpp", "ChatWindow.qml"]:
        require(token in desktop_cmake, f"desktop CMake missing {token}")
    require(
        "CopyMilesAgentSidecar" in desktop_cmake
        and "add_dependencies(MilesEdgeworthDesktop CopyMilesAgentSidecar)" in desktop_cmake,
        "desktop CMake must copy miles-agent through a sidecar target so provider-only rebuilds update the app bundle",
    )

    require("go 1.22" in go_mod, "agent-core go.mod must target Go 1.22")
    require("find_program(GO_EXECUTABLE go)" in agent_cmake, "agent-core CMake must find go")
    require("/health" in server_go, "sidecar must expose /health")
    require("/v1/chat/messages" in server_go, "sidecar must expose chat messages endpoint")
    require("text/event-stream" in server_go, "chat endpoint must use SSE")
    require("s.provider == nil" in service_go, "chat service must handle an unconfigured provider")
    require("RUN_ERROR" in service_go, "chat service must emit RUN_ERROR when provider is unconfigured")
    require("httptest.NewServer" in server_test, "Go tests must cover HTTP server")

    for token in ["ChatStreamEvent", "ChatStreamEventParser", "ingest", "QJsonDocument"]:
        require(token in parser_h + parser_cpp, f"SSE parser missing {token}")

    for token in [
        "QNetworkAccessManager",
        "QProcess",
        "sendMessage",
        "cancelCurrentReply",
        "openWindowRequested",
        "requestExpression",
        "miles.pet.expression.requested",
    ]:
        require(token in controller_h + controller_cpp, f"ChatController missing {token}")

    require("import QtQuick.Controls" in chat_qml, "ChatWindow must use Qt Quick Controls")
    require("App.ChatController.sendMessage" in chat_qml, "ChatWindow must send through ChatController")
    require("onOpenWindowRequested" in chat_qml, "ChatWindow must react to controller open signal")
    require("flags: Qt.Window" in chat_qml, "ChatWindow must be a top-level Qt window")
    require(
        "App.DesktopShell.setChatWindowDockVisible" not in chat_qml,
        "ChatWindow visibility must not switch macOS activation policy because it can move Spaces",
    )
    require("id: compactComposer" in chat_qml, "ChatWindow composer must be addressable by id")
    require(
        "onSubmitRequested: function(text)" in chat_qml
        and "App.ChatController.sendMessage(text)" in chat_qml,
        "ChatWindow composer submit action must send through ChatController",
    )
    chat_input_qml = chat_qml + composer_qml
    require("placeholderTextColor" in chat_input_qml, "ChatWindow input must set readable placeholder color")
    require("background: Rectangle" in chat_input_qml, "ChatWindow input and buttons must use explicit backgrounds")
    require("contentItem: Text" in chat_input_qml, "ChatWindow buttons must use readable explicit text content")
    require("ChatControllerForeign::s_instance" in main_cpp, "main must expose ChatController singleton")
    require(
        "QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership)" in controller_h,
        "ChatController singleton must keep C++ ownership so QQmlEngine does not delete the stack instance",
    )
    require("#include <QQuickStyle>" in main_cpp, "main must include QQuickStyle for chat controls styling")
    require(
        'QQuickStyle::setStyle("Basic")' in main_cpp,
        "main must use a customizable Qt Quick Controls style before loading ChatWindow",
    )
    require("setChatWindowDockVisible" not in shell_h + shell_cpp,
            "DesktopShellController must not expose chat Dock visibility control")
    require(
        "setChatWindow(QWindow *window)" in shell_h + shell_cpp
        and "m_chatWindow" in shell_h + shell_cpp,
        "DesktopShellController must retain the ChatWindow QWindow for macOS Space behavior",
    )
    require(
        "setChatBubbleWindow(QWindow *window)" in shell_h + shell_cpp
        and "m_chatBubbleWindow" in shell_h + shell_cpp,
        "DesktopShellController must retain the ChatBubbleWindow QWindow for macOS Space behavior",
    )
    require(
        "prepareChatWindowForOpen()" in shell_h + shell_cpp
        and "App.DesktopShell.prepareChatWindowForOpen()" in chat_qml,
        "ChatWindow open path must prepare the native window before activation",
    )
    require(
        "requestActivate()" not in chat_qml,
        "ChatWindow QML must not request app activation directly because it can switch macOS Spaces",
    )
    require(
        "applyMacCompanionWindowBehavior" in shell_cpp + mac_behavior_h + mac_behavior_mm
        and "prepareMacCompanionWindowForOpen" in shell_cpp + mac_behavior_h + mac_behavior_mm,
        "macOS platform layer must expose companion behavior for chat windows",
    )
    require(
        "setMacApplicationDockVisible(bool visible)" in mac_behavior_h + mac_behavior_mm,
        "macOS platform layer must expose startup application activation policy control",
    )
    require(
        "NSApplicationActivationPolicyAccessory" in mac_behavior_mm,
        "macOS app must be able to start in accessory activation policy",
    )
    require(
        "setMacApplicationDockVisible(false)" in main_cpp,
        "main must start macOS in accessory mode; chat visibility must not change activation policy",
    )
    require("loadFromModule(\"MilesEdgeworth\", \"ChatWindow\")" in main_cpp, "main must load ChatWindow QML")
    require(
        "qobject_cast<QWindow *>(chatEngine.rootObjects().at(0))" in main_cpp
        and "shellController.setChatWindow(chatWindow)" in main_cpp,
        "main must register ChatWindow with DesktopShellController",
    )
    require(
        "qobject_cast<QWindow *>(chatEngine.rootObjects().at(2))" in main_cpp
        and "shellController.setChatBubbleWindow(chatBubbleWindow)" in main_cpp,
        "main must register ChatBubbleWindow with DesktopShellController",
    )
    require("聊天" in menu_cpp, "native pet context menu must include chat entry")
    require('"error"' in manifest, "Miles manifest must expose error state for chat failures")
    require('"recipe": "thinking.holdUntilCancelled", "allowedStates": ["thinking"]' in manifest,
            "neutral thinking must map to phased thinking recipe")
    require("miles.pet.expression.requested" in parser_smoke, "parser smoke must cover custom expression events")
    require("controller.applyStreamEvent" in controller_smoke, "controller smoke must cover event application")
    require("Phase 2.0" in phase_record, "phase record must document Phase 2.0")
    require("Phase 2.0 AI Chat MVP 骨架" in index_doc, "v2 index must link the Phase 2.0 record")

    print("phase 2.0 ai chat mvp contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
