#!/usr/bin/env python3
"""检查 Phase 0.68 的运行时去 Miles 硬编码边界。"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    event_h = read("apps/desktop/src/pet/events/PetEvent.h")
    bridge_cpp = read("apps/desktop/src/pet/events/PetEventBridge.cpp")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    trigger_engine_cpp = read("apps/desktop/src/pet/behavior/BehaviorTriggerEngine.cpp")

    capabilities = manifest.get("capabilities", {})
    rest = capabilities.get("rest", {})
    require(rest.get("enterRecipe") == "sleep.enterLoopExit", "manifest 应声明 rest.enterRecipe")
    require(rest.get("loopAction") == "sleep", "manifest 应声明 rest.loopAction")

    triggers = manifest.get("behaviorTriggers", {})
    require("runtime.started" in triggers, "manifest 应声明 runtime.started trigger")
    require("action.completed" in triggers, "manifest 应声明 action.completed trigger")

    action_completed_entries = triggers["action.completed"].get("entries", [])
    require(
        any(entry.get("when", {}).get("action") == "walk" and entry.get("pool") == "walk.finished" for entry in action_completed_entries),
        "action.completed 应声明 walk 完成后的候选池",
    )
    require(
        any(entry.get("when", {}).get("action") == "run" and entry.get("pool") == "run.finished" for entry in action_completed_entries),
        "action.completed 应声明 run 完成后的候选池",
    )

    movement_facing_map = manifest.get("movementFacingMap", {})
    for direction, facing in {
        "east": "right",
        "northEast": "right",
        "southEast": "right",
        "west": "left",
        "northWest": "left",
        "southWest": "left",
    }.items():
        require(movement_facing_map.get(direction) == facing, f"movementFacingMap 缺少 {direction} -> {facing}")

    require(
        manifest.get("actions", {}).get("briefcase_in", {}).get("blocksPointerInteraction") is True,
        "启动入场 action 应通过 blocksPointerInteraction 禁用鼠标交互",
    )

    for token in [
        "RestCapabilityDefinition",
        "CapabilityDefinition",
        "movementFacingMap",
        "blocksPointerInteraction",
    ]:
        require(token in manifest_h, f"SkinManifest 缺少字段：{token}")

    for token in [
        "manifest.capabilities.rest.enterRecipeId",
        "manifest.capabilities.rest.exitRecipeId",
        "manifest.capabilities.rest.loopActionId",
        "manifest.movementFacingMap",
        "action.blocksPointerInteraction",
    ]:
        require(token in loader_cpp, f"SkinManifestLoader 未解析字段：{token}")

    for token in [
        "RuntimeStarted",
        "ActionCompleted",
        "runtimeStarted",
        "actionCompleted",
    ]:
        require(token in event_h, f"PetEvent 缺少运行时事件：{token}")

    for token in [
        "PetEventType::RuntimeStarted",
        "PetEventType::ActionCompleted",
        "manifest.capabilities.rest.enterRecipeId",
        "manifest.capabilities.rest.loopActionId",
    ]:
        require(token in pipeline_cpp, f"InteractionPipeline 缺少 Phase 0.68 处理：{token}")

    require("entry.when.actionId" in trigger_engine_cpp, "BehaviorTriggerEngine 应支持 entry 级 action 条件")

    for token in [
        '"sleep"',
        "sleep.enterLoopExit",
        "briefcase_in",
        "startup.briefcase",
        "walk.finished",
        "run.finished",
        'QStringLiteral("east")',
        'QStringLiteral("west")',
        "followUpPoolForCompletedAction",
    ]:
        require(token not in pet_runtime_cpp, f"PetRuntime.cpp 不应继续硬编码：{token}")

    for token in [
        'm_currentActionId == "sleep"',
        "followUpPoolForCompletedAction",
        'm_currentMovementDirection = "east"',
    ]:
        require(token not in pet_runtime_h, f"PetRuntime.h 不应继续硬编码：{token}")

    for token in [
        'currentActionId == "sleep"',
        "sleep.enterLoopExit",
    ]:
        require(token not in pipeline_cpp, f"InteractionPipeline 不应继续硬编码 sleep 行为：{token}")
        require(token not in bridge_cpp, f"PetEventBridge 不应继续硬编码 sleep 行为：{token}")

    guardrail = read("tests/check_phase_0_65_architecture_guardrails.cmake")
    for token in [
        "sleep.enterLoopExit",
        "briefcase_in",
        "startup.briefcase",
        "walk.finished",
        "run.finished",
        'QStringLiteral("east")',
        'QStringLiteral("west")',
    ]:
        require(token not in guardrail, f"Phase 0.68 已收口项不应继续留在 0.65 expected-fail 门禁：{token}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
