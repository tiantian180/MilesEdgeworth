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
        "setAutoMovementEnabled(restoreAutoMovementEnabled)",
    ]:
        require(token in runtime_h + runtime_cpp, f"PetRuntime motion bridge missing {token}")
    require("setState(QStringLiteral(\"moving\"))" not in runtime_cpp,
            "moving state must not use setState side-effect path")
    require("playLocomotion" in runtime_cpp and "QStringLiteral(\"loop\")" in runtime_cpp,
            "target locomotion must override loop mode")

    require("cancelMotionForDrag" in bridge_cpp,
            "drag start must cancel active target motion")
    require("motionPositionChanged" in main_cpp and "movePetWindowToMotionClampedPosition" in main_cpp,
            "main must connect motion positions to DesktopShellController")
    require("petMotionScreenGeometry" in main_cpp,
            "main must inject screen geometry into PetRuntime")

    for token in ["toolCallId", "toolName", "toolArgs"]:
        require(token in stream_h and token in stream_cpp,
                f"ChatStreamEvent must parse {token}")
    for token in [
        "EXECUTING_TOOL",
        "executingTool",
        "handleToolCall",
        "postToolResult",
        "kToolResultUrl",
        "MILESEDGEWORTH_TOOL_RESULT_URL",
        "kMotionToolTimeoutMs",
        "requestMotion",
        "Miles 正在移动",
    ]:
        require(token in chat_h + chat_cpp,
                f"ChatController tool execution missing {token}")
    require("RUN_FINISHED" in chat_cpp and "hasPendingToolCall()" in chat_cpp,
            "ChatController must defend against residual RUN_FINISHED while a tool is pending")

    for token in [
        "ToolDefinition",
        "ToolCall",
        "ToolCalls",
        "ToolCallID",
        "ToolName",
        "ToolArgs",
    ]:
        require(token in provider_go, f"chat provider model missing {token}")
    for token in [
        "tool_calls",
        "makeChatTools",
        "parallel_tool_calls",
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

    compact_tools = tools_go.replace(" ", "")
    require('"enum":["moveTo","moveBy"]' in compact_tools,
            "pet_motion schema must expose moveTo and moveBy only")
    require('"stop"' not in tools_go,
            "pet_motion schema must not expose stop")
    require("normalized decimal" in tools_go and "0.5" in tools_go and "not 50" in tools_go,
            "pet_motion schema must describe normalized decimal coordinates and avoid 0-100 ambiguity")
    require('"minimum":-1' in compact_tools and '"maximum":1' in compact_tools,
            "pet_motion schema must bound x/y to normalized decimal ranges")

    require("/v1/chat/tool-result" in api_go,
            "API server must expose tool result endpoint")
    require("SubmitToolResult" in api_go,
            "tool result endpoint must route into chat service")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
