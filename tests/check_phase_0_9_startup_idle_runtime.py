#!/usr/bin/env python3
"""检查 Phase 0.9 的启动序列和旧版 idle 手感骨架。

旧版桌宠启动时会先播放公文包入场，再进入站立；站立状态还会随机触发
转身和若干 once 小动作。本检查先守住这些行为在 v2 manifest 和运行时里的入口。
"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def action_loop_mode(actions: dict, action_id: str) -> str:
    action = actions.get(action_id, {})
    return action.get("loopMode", "")


def main() -> int:
    manifest = json.loads((ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json").read_text(encoding="utf-8"))
    actions = manifest.get("actions", {})
    recipes = manifest.get("recipes", {})
    action_pools = manifest.get("actionPools", {})
    behavior_triggers = manifest.get("behaviorTriggers", {})

    for action_id in [
        "briefcase_in",
        "briefcase_stop",
        "turn_around",
        "idle_thinking_once",
        "idle_tapping_head",
        "idle_shrug",
        "idle_check_watch",
        "idle_pointing",
    ]:
        require(action_id in actions, f"manifest actions 缺少 {action_id}")

    require(action_loop_mode(actions, "briefcase_in") == "once", "briefcase_in 应播放一次后进入下一步")
    require(action_loop_mode(actions, "briefcase_stop") == "once", "briefcase_stop 应播放一次后进入站立")
    require(action_loop_mode(actions, "turn_around") == "onceThenIdle", "turn_around 应播放一次后回 idle")
    require(action_loop_mode(actions, "idle_thinking_once") == "onceThenIdle", "随机 idle 的思考动作必须是一次性版本")
    require(action_loop_mode(actions, "thinking") == "loop", "agent thinking 仍应保留循环版本")

    turn = actions["turn_around"]
    require(turn.get("facingAfter", {}).get("right") == "left", "右朝向转身后应变为 left")
    require(turn.get("facingAfter", {}).get("left") == "right", "左朝向转身后应变为 right")

    startup = recipes.get("startup.briefcase")
    require(startup, "recipes 缺少 startup.briefcase")
    require(startup.get("scope") == "startup", "startup.briefcase 应声明为 startup recipe")
    startup_steps = startup.get("steps", [])
    require([step.get("action") for step in startup_steps] == ["briefcase_in", "briefcase_stop", "idle_stand"], "启动 recipe 应按 briefcase_in -> briefcase_stop -> idle_stand 编排")
    runtime_started_entries = behavior_triggers.get("runtime.started", {}).get("entries", [])
    require(any(entry.get("recipe") == "startup.briefcase" for entry in runtime_started_entries), "runtime.started trigger 应选择 startup.briefcase")

    require(recipes.get("idle.randomThinking", {}).get("action") == "idle_thinking_once", "idle.randomThinking 应使用一次性思考动作")
    require(recipes.get("turn.once", {}).get("action") == "turn_around", "turn.once 应映射到 turn_around")

    idle_entries = action_pools.get("idle.random", {}).get("entries", [])
    idle_recipe_ids = {entry.get("recipe") for entry in idle_entries}
    for recipe_id in [
        "idle.randomThinking",
        "turn.once",
        "idle.tappingHead",
        "idle.shrug",
        "idle.checkWatch",
        "idle.pointing",
    ]:
        require(recipe_id in idle_recipe_ids, f"idle.random 缺少 {recipe_id}")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    for alias in [
        "briefcase-in-right.gif",
        "briefcase-stop-right.gif",
        "turn-right-to-left.gif",
        "turn-left-to-right.gif",
        "idle-thinking-once-right.gif",
        "idle-thinking-once-left.gif",
        "idle-tapping-head-right.gif",
        "idle-tapping-head-left.gif",
        "idle-shrug-right.gif",
        "idle-shrug-left.gif",
        "idle-check-watch-right.gif",
        "idle-check-watch-left.gif",
        "idle-pointing-right.gif",
        "idle-pointing-left.gif",
    ]:
        require(f'alias="{alias}"' in qrc, f"qrc 缺少 {alias}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    for token in [
        "Q_INVOKABLE void startStartupSequence",
        "applyFacingAfterCurrentAction",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")
    require("facingAfter" in manifest_h, "ActionDefinition 应保存 facingAfter")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    for token in [
        "PetEvent::runtimeStarted()",
        "submitRuntimeEvent",
        "startStartupSequence",
        "applyFacingAfterCurrentAction",
        "action.facingAfter",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")
    require("action.facingAfter.insert" in loader_cpp, "SkinManifestLoader 应解析 facingAfter")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "m_eventBridge->submitIdleLoopFinished()",
        "m_runtime->currentRecipeId().isEmpty()",
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow.cpp 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_9_startup_idle_runtime" in root_cmake, "CTest 未注册 Phase 0.9 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
