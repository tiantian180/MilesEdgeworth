#!/usr/bin/env python3
"""Check the Phase 2.4 phased animation and chat segment contract."""

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
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    pacer_h = read("apps/desktop/src/chat/ChatTextPacer.h")
    pacer_cpp = read("apps/desktop/src/chat/ChatTextPacer.cpp")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    provider_go = read("apps/agent-core/internal/chat/openai/provider.go")
    provider_test = read("apps/agent-core/internal/chat/openai/provider_test.go")
    root_cmake = read("CMakeLists.txt")
    debt_doc = read("docs/v2/参考资料/技术债务与评审待办.md")
    readme = read("README.md")

    actions = manifest.get("actions", {})
    recipes = manifest.get("recipes", {})
    require(actions["objecting"]["loopMode"] == "onceThenHold", "objecting must be onceThenHold")
    require(actions["bow"]["loopMode"] == "onceThenHold", "bow must be onceThenHold")
    require(set(actions["thinking"].get("phases", {}).keys()) >= {"enter", "loop", "exit"},
            "thinking must expose enter/loop/exit phases")
    require(set(actions["talking"].get("phases", {}).keys()) >= {"enter", "loop", "exit"},
            "talking must expose enter/loop/exit phases")
    require(actions["talking"]["phases"]["enter"]["variants"]["right"]["frameRange"] == [1, 4],
            "talking enter must use crossed frames 1-4")
    require(actions["talking"]["phases"]["loop"]["variants"]["right"]["frameRange"] == [5, 8],
            "talking loop must use crossed frames 5-8")
    require(actions["talking"]["phases"]["exit"]["variants"]["right"]["frameRange"] == [9, 11],
            "talking exit must use crossed frames 9-11")
    require({"action": "talking", "allowedStates": ["speaking"]} in
            manifest["expressionMappings"]["neutral"]["actions"],
            "neutral speaking must map to talking")
    require("thinking.holdUntilCancelled" in recipes, "manifest must define thinking.holdUntilCancelled")
    require(any(step.get("duration") == "runtime"
                for step in recipes["thinking.holdUntilCancelled"].get("steps", [])),
            "thinking recipe must include runtime-controlled loop step")

    for token in [
        "reloadActiveSkinPreservingPlayback",
        "setSuppressAutoIdle",
        "m_cleanFinishCallback",
        "requestCleanFinishAndNotify",
        "cancelCleanFinishNotification",
        "kCleanFinishSafetyMs = 2000",
        "kAutoIdleAfterCleanFinishMs = 3000",
        "currentFrameStart",
        "currentFrameEnd",
    ]:
        require(token in runtime_h + runtime_cpp, f"runtime missing {token}")

    require("onceThenHold" in surface_cpp and "loadManualFrameRange" in surface_cpp
            and "m_manualFrameTimer" in surface_h,
            "native surface must handle onceThenHold and play frameRange phases without relying on QMovie seeking")

    for token in [
        "append(const QString &text, quint64 streamId = 0, int segmentId = -1)",
        "segmentDrained",
        "pacerEmpty",
        "pendingCountForSegment",
    ]:
        require(token in pacer_h + pacer_cpp, f"pacer missing {token}")

    for token in [
        "ExpressionSegment",
        "m_segmentQueue",
        "m_activeSegmentId",
        "m_streamFinished",
        "m_startTimeout",
        "kGateTimeoutMs = 2000",
        "kStartTimeoutMs = 3000",
        "miles.pet.lifecycle",
        "maybeAdvanceGate",
        "maybeFinishWaitingForAnimationEnd",
    ]:
        require(token in controller_h + controller_cpp, f"ChatController missing {token}")

    require('"miles.pet.lifecycle"' in provider_go, "Go provider must emit lifecycle event")
    require('"miles.pet.expression.requested"' in provider_go, "Go provider must still emit expression events")
    require('"state":         "idle"' not in provider_go, "Go provider must not emit idle expression on stream end")
    require("miles.pet.lifecycle" in provider_test, "Go provider tests must cover lifecycle")

    require("check_phase_2_4_phased_animation" in root_cmake,
            "CTest must register Phase 2.4 contract check")
    require("[P2.4]" not in debt_doc,
            "P2.4 debt entries should be removed or rewritten after implementation")
    require("Phase 2.4" in readme,
            "README should link the Phase 2.4 stage record")

    print("phase 2.4 phased animation contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
