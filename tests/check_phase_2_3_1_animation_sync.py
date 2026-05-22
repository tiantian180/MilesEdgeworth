#!/usr/bin/env python3
"""Check the Phase 2.3.1 animation-text sync contract."""

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
    pacer_h = read("apps/desktop/src/chat/ChatTextPacer.h")
    pacer_cpp = read("apps/desktop/src/chat/ChatTextPacer.cpp")
    pacer_smoke = read("apps/desktop/tests/chat_text_pacer_smoke.cpp")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    controller_smoke = read("apps/desktop/tests/chat_controller_smoke.cpp")
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    runtime_smoke = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")
    stage_doc = read("docs/v2/阶段记录/Phase 2.3.1 动画-文字同步.md")
    index_doc = read("docs/v2/文档索引.md")

    # ChatTextPacer
    require("class ChatTextPacer" in pacer_h, "ChatTextPacer class must exist")
    require("void append(" in pacer_h, "ChatTextPacer must expose append()")
    require("setMsPerChar" in pacer_h, "ChatTextPacer must expose setMsPerChar()")
    require("msPerChar()" in pacer_h, "ChatTextPacer must expose msPerChar() getter")
    require("pendingCount()" in pacer_h, "ChatTextPacer must expose pendingCount() for tests")
    require("void chunkReady" in pacer_h, "ChatTextPacer must declare chunkReady signal")
    require("QTimer" in pacer_cpp, "ChatTextPacer must drive emission with QTimer")
    require("maxbacklog" in pacer_cpp.lower() or "backlog" in pacer_cpp.lower(),
            "ChatTextPacer must implement backlog catch-up")

    # ChatController state machine
    require("enum class ChatPhase" in controller_h, "ChatController must declare ChatPhase enum")
    require("IDLE" in controller_h and "BUFFERING_FOR_START" in controller_h
            and "STREAMING" in controller_h and "GATED" in controller_h
            and "WAITING_FOR_ANIMATION_END" in controller_h,
            "ChatPhase must include all 5 design states")
    require("ChatTextPacer" in controller_h, "ChatController must own a ChatTextPacer")
    require("transitionTo" in controller_h, "ChatController must expose a transitionTo helper")
    require("handleCleanFinishReady" in controller_h,
            "ChatController must handle PetRuntime cleanFinishReady callback")
    require("handleBoundaryReached" in controller_h,
            "ChatController must handle PetRuntime boundaryReached callback")
    require("handleGateTimeout" in controller_h,
            "ChatController must handle GATED safety timeout")
    require("drainHoldBufferToPacer" in controller_h,
            "ChatController must drain its hold buffer through the pacer")
    require("appendChunkToCurrentMessage" in controller_h,
            "ChatController must append pacer chunks via a dedicated method")
    require("requestBoundaryAndNotify" in controller_cpp,
            "ChatController must call requestBoundaryAndNotify on PetRuntime")
    require("requestCleanFinishAndNotify" in controller_cpp,
            "ChatController must call requestCleanFinishAndNotify on PetRuntime")
    require("chat expression requested" in controller_cpp and "qInfo" in controller_cpp,
            "ChatController should log parsed expression events for provider-vs-runtime diagnosis")

    # PetRuntime API
    require("requestBoundaryAndNotify" in runtime_h,
            "PetRuntime must declare requestBoundaryAndNotify")
    require("requestCleanFinishAndNotify" in runtime_h,
            "PetRuntime must declare requestCleanFinishAndNotify")
    require("std::function" in runtime_h,
            "PetRuntime notify API must take std::function callbacks")
    require("drainPendingNotifications" in runtime_cpp,
            "PetRuntime must drain pending notifications at animation boundaries")

    # Tests
    require("ChatTextPacer" in pacer_smoke,
            "pacer smoke test must exercise ChatTextPacer")
    require("msPerChar" in pacer_smoke,
            "pacer smoke test must cover msPerChar")
    require("backlog" in pacer_smoke.lower(),
            "pacer smoke test must cover backlog catch-up")
    require("BUFFERING_FOR_START" in controller_smoke
            or "ChatPhase" in controller_smoke,
            "controller smoke test must exercise the state machine")
    require("GATED" in controller_smoke
            or "expression switch" in controller_smoke.lower(),
            "controller smoke test must exercise mid-run expression gating")
    require("requestBoundaryAndNotify" in runtime_smoke,
            "runtime smoke test must exercise requestBoundaryAndNotify")

    # CMake
    require("ChatTextPacer.cpp" in desktop_cmake,
            "desktop CMake must list ChatTextPacer.cpp")
    require("ChatTextPacerSmoke" in desktop_cmake,
            "desktop CMake must register the ChatTextPacerSmoke target")
    require("chat_text_pacer_smoke" in desktop_cmake,
            "desktop CMake must register chat_text_pacer_smoke ctest")
    require("check_phase_2_3_1_animation_sync" in root_cmake,
            "root CMake must register the Phase 2.3.1 contract check")

    # Docs
    require("Phase 2.3.1" in stage_doc, "Phase 2.3.1 stage record must exist")
    require("Phase 2.3.1" in index_doc, "doc index must link Phase 2.3.1 record")

    print("phase 2.3.1 animation-text sync contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
