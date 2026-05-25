#!/usr/bin/env python3
"""Check the Phase 2.4 phased animation and chat segment contract."""

from __future__ import annotations

import json
import re
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


def walk_dispatch_objects(value):
    if isinstance(value, dict):
        yield value
        for item in value.values():
            yield from walk_dispatch_objects(item)
    elif isinstance(value, list):
        for item in value:
            yield from walk_dispatch_objects(item)


def main() -> int:
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    pacer_h = read("apps/desktop/src/chat/ChatTextPacer.h")
    pacer_cpp = read("apps/desktop/src/chat/ChatTextPacer.cpp")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    provider_go = read("apps/agent-core/internal/chat/openai/provider.go")
    provider_test = read("apps/agent-core/internal/chat/openai/provider_test.go")
    root_cmake = read("CMakeLists.txt")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    debt_doc = read("docs/v2/参考资料/技术债务与评审待办.md")
    readme = read("README.md")

    actions = manifest.get("actions", {})
    clips = manifest.get("clips", {})
    recipes = manifest.get("recipes", {})
    legacy_dispatch_types = {"pool", "recipe", "action", "none", "returnToIdle", "toggleFacing"}
    bad_dispatch_types = [
        value.get("type")
        for value in walk_dispatch_objects(manifest)
        if isinstance(value.get("type"), str) and value.get("type") in legacy_dispatch_types
    ]
    require(not bad_dispatch_types, "Miles manifest v4 must use key dispatch instead of dispatch type fields")
    require(actions["objecting"]["loopMode"] == "onceThenHold", "objecting must be onceThenHold")
    require(actions["bow"]["loopMode"] == "onceThenHold", "bow must be onceThenHold")
    require(set(actions["thinking"].get("phases", {}).keys()) >= {"enter", "loop", "exit"},
            "thinking must expose enter/loop/exit phases")
    require(set(actions["talking"].get("phases", {}).keys()) >= {"enter", "loop", "exit"},
            "talking must expose enter/loop/exit phases")
    expected_clips = {
        "thinking.enter.right": ("file:assets/body/gestures/thinking-right.gif", [1, 4]),
        "thinking.loop.right": ("file:assets/body/gestures/thinking-right.gif", [5, 8]),
        "thinking.exit.right": ("file:assets/body/gestures/thinking-right.gif", [44, 47]),
        "talking.enter.right": ("file:assets/body/interaction/crossed-right.gif", [1, 4]),
        "talking.loop.right": ("file:assets/body/interaction/crossed-right.gif", [5, 8]),
        "talking.exit.right": ("file:assets/body/interaction/crossed-right.gif", [9, 11]),
    }
    for clip_id, (source, frame_range) in expected_clips.items():
        require(clips.get(clip_id, {}).get("source") == source, f"{clip_id} must declare source {source}")
        require(clips.get(clip_id, {}).get("frameRange") == frame_range,
                f"{clip_id} must declare frameRange {frame_range}")
    require(actions["talking"]["phases"]["enter"]["variants"]["right"]["clip"] == "talking.enter.right",
            "talking enter must reference generated clip")
    require(actions["talking"]["phases"]["loop"]["variants"]["right"]["clip"] == "talking.loop.right",
            "talking loop must reference generated clip")
    require(actions["talking"]["phases"]["exit"]["variants"]["right"]["clip"] == "talking.exit.right",
            "talking exit must reference generated clip")
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
    ]:
        require(token in runtime_h + runtime_cpp, f"runtime missing {token}")

    for token in ["currentFrameStart", "currentFrameEnd"]:
        require(token not in runtime_h, f"runtime must not expose {token}")
    for token in ["hasFrameRange", "frameStart", "frameEnd"]:
        require(not re.search(rf"\b{token}\b", manifest_h),
                f"manifest runtime AnimationVariant must not keep {token}")
    for token in ["loadManualFrameRange", "m_manualFrameTimer", "QImageReader", "m_manualFrames"]:
        require(token not in surface_h + surface_cpp,
                f"native surface must not keep runtime frameRange playback token {token}")
    require("onceThenHold" in surface_cpp,
            "native surface must still handle onceThenHold full-GIF playback")
    require("GenerateMilesClips" in desktop_cmake and "split_manifest_clips.py" in desktop_cmake,
            "desktop build must generate built-in Miles pre-cut clips")

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
        "requestGateCleanFinishIfTextDrained",
        "requestFinishCleanFinishIfPacerEmpty",
        "canDelayCleanFinishForCurrentAnimation",
    ]:
        require(token in controller_h + controller_cpp, f"ChatController missing {token}")
    can_delay_start = controller_cpp.index("bool ChatController::canDelayCleanFinishForCurrentAnimation() const")
    can_delay_end = controller_cpp.index("void ChatController::requestGateCleanFinishIfTextDrained")
    can_delay_body = controller_cpp[can_delay_start:can_delay_end]
    require(re.search(
        r"return\s+m_runtime\s*==\s*nullptr\s*\|\|\s*\(\s*"
        r"m_runtime->currentLoopMode\(\)\s*==\s*QStringLiteral\(\"loop\"\)\s*"
        r"&&\s*!m_runtime->currentAutoReturnToIdle\(\)\s*\)\s*;",
        can_delay_body,
        re.S,
    ), "ChatController must delay cleanFinish only for missing runtime or loop-safe animations")
    gate_finish_start = controller_cpp.index("void ChatController::requestGateCleanFinishIfTextDrained")
    gate_finish_end = controller_cpp.index("void ChatController::requestFinishCleanFinishIfPacerEmpty")
    gate_finish_body = controller_cpp[gate_finish_start:gate_finish_end]
    require("!m_textDrained && canDelayCleanFinishForCurrentAnimation()" in gate_finish_body,
            "ChatController gate cleanFinish must wait for text drain when the animation can keep looping")
    require("requestCleanFinishForCurrentStream()" in gate_finish_body,
            "ChatController gate cleanFinish helper must request the current stream cleanFinish")
    finish_start = controller_cpp.index("void ChatController::requestFinishCleanFinishIfPacerEmpty")
    finish_end = controller_cpp.index("void ChatController::requestCleanFinishForStream")
    finish_body = controller_cpp[finish_start:finish_end]
    require("!m_pacerEmpty && canDelayCleanFinishForCurrentAnimation()" in finish_body,
            "ChatController final cleanFinish must wait for pacer empty when the animation can keep looping")
    require("requestCleanFinishForCurrentStream()" in finish_body,
            "ChatController final cleanFinish helper must request the current stream cleanFinish")
    enter_gate_start = controller_cpp.index("void ChatController::enterGateForNextSegment")
    enter_gate_end = controller_cpp.index("void ChatController::maybeAdvanceGate")
    enter_gate_body = controller_cpp[enter_gate_start:enter_gate_end]
    require("requestGateCleanFinishIfTextDrained()" in enter_gate_body,
            "ChatController enterGateForNextSegment must use the gated cleanFinish helper")
    require("requestCleanFinishForCurrentStream()" not in enter_gate_body,
            "ChatController enterGateForNextSegment must not eagerly request cleanFinish")
    maybe_advance_start = controller_cpp.index("void ChatController::maybeAdvanceGate")
    maybe_advance_end = controller_cpp.index("void ChatController::maybeFinishWaitingForAnimationEnd")
    maybe_advance_body = controller_cpp[maybe_advance_start:maybe_advance_end]
    require("requestFinishCleanFinishIfPacerEmpty()" in maybe_advance_body,
            "ChatController WAITING entry must use the pacer-aware final cleanFinish helper")
    send_message_start = controller_cpp.index("void ChatController::sendMessageInConversation")
    send_message_end = controller_cpp.index("void ChatController::cancelCurrentReply")
    send_message_body = controller_cpp[send_message_start:send_message_end]
    require('requestPetExpression(QStringLiteral("thinking"), QStringLiteral("neutral"));' not in send_message_body,
            "sendMessageInConversation must wait for lifecycle thinking instead of pre-starting thinking")

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
