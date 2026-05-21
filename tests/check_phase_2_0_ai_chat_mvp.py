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
    mock_provider_go = read("apps/agent-core/internal/chat/mock_provider.go")
    server_test = read("apps/agent-core/internal/api/server_test.go")
    parser_h = read("apps/desktop/src/chat/ChatStreamEvent.h")
    parser_cpp = read("apps/desktop/src/chat/ChatStreamEvent.cpp")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    main_cpp = read("apps/desktop/src/main.cpp")
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

    require("go 1.22" in go_mod, "agent-core go.mod must target Go 1.22")
    require("find_program(GO_EXECUTABLE go)" in agent_cmake, "agent-core CMake must find go")
    require("/health" in server_go, "sidecar must expose /health")
    require("/v1/chat/messages" in server_go, "sidecar must expose chat messages endpoint")
    require("text/event-stream" in server_go, "chat endpoint must use SSE")
    require("RUN_STARTED" in mock_provider_go, "mock provider must emit RUN_STARTED")
    require("TEXT_MESSAGE_CONTENT" in mock_provider_go, "mock provider must emit token content")
    require("miles.pet.expression.requested" in mock_provider_go, "mock provider must emit pet expression custom events")
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
    require("ChatControllerForeign::s_instance" in main_cpp, "main must expose ChatController singleton")
    require("loadFromModule(\"MilesEdgeworth\", \"ChatWindow\")" in main_cpp, "main must load ChatWindow QML")
    require("聊天" in menu_cpp, "native pet context menu must include chat entry")
    require('"error"' in manifest, "Miles manifest must expose error state for chat failures")
    require('"action": "thinking", "allowedStates": ["thinking"]' in manifest, "neutral thinking must map to thinking action")
    require("miles.pet.expression.requested" in parser_smoke, "parser smoke must cover custom expression events")
    require("controller.applyStreamEvent" in controller_smoke, "controller smoke must cover event application")
    require("Phase 2.0" in phase_record and "mock provider" in phase_record, "phase record must document Phase 2.0")
    require("Phase 2.0 AI Chat MVP 骨架" in index_doc, "v2 index must link the Phase 2.0 record")

    print("phase 2.0 ai chat mvp contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
