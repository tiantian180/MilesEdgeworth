#!/usr/bin/env python3
"""检查 v2 当前对旧版核心手感目标的覆盖度。

这个测试不是替代真实试玩，而是把 active goal 里已经明确列出的能力收束到
同一张清单，防止后续重构时漏掉随机 idle、走跑、点击、双击、睡眠、徽章等基础体验。
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


def main() -> int:
    manifest = json.loads((ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json").read_text(encoding="utf-8"))
    actions = manifest.get("actions", {})
    recipes = manifest.get("recipes", {})
    action_pools = manifest.get("actionPools", {})

    for action_id in [
        "idle_stand",
        "turn_around",
        "walk",
        "run",
        "tea",
        "tea_alt",
        "sleep",
        "drag_crouch",
        "drag_stand_up_full",
        "drag_stand_up_quick",
        "crossed",
        "objecting",
        "bow",
        "pickup_badge",
    ]:
        require(action_id in actions, f"manifest actions 缺少核心动作 {action_id}")

    idle_entries = action_pools.get("idle.random", {}).get("entries", [])
    idle_recipes = {entry.get("recipe") for entry in idle_entries}
    for recipe_id in ["idle.randomThinking", "turn.once", "idle.tappingHead", "idle.shrug", "idle.checkWatch", "idle.pointing"]:
        require(recipe_id in idle_recipes, f"idle.random 缺少随机 idle 候选 {recipe_id}")
    require(any(recipe_id and recipe_id.startswith("walk.") for recipe_id in idle_recipes), "idle.random 应包含 walk 候选")
    require(any(recipe_id and recipe_id.startswith("run.") for recipe_id in idle_recipes), "idle.random 应包含 run 候选")

    movement_directions = manifest.get("movementDirections", [])
    require(len(movement_directions) == 8, "移动方向应覆盖旧版 8 方向")
    for action_id in ["walk", "run"]:
        variants = actions[action_id].get("variants", {})
        require(set(movement_directions) <= set(variants.keys()), f"{action_id} 应覆盖所有移动方向")
        require(all("movement" in variants[direction] for direction in movement_directions), f"{action_id} 每个方向都应有 movement 增量")

    click_pools = manifest.get("clickBehaviors", {}).get("singleClick", [])
    require([entry.get("pool") for entry in click_pools] == [
        "click.face",
        "click.head",
        "click.upperArm",
        "click.forearm",
        "click.chest",
        "click.bellyBow",
        "click.bellyPointingArea",
        "click.legsBackArea",
        "click.legsLookDownArea",
        "click.fallback",
    ], "单击分区顺序应覆盖旧版区域和肚子/腿部子区域")

    double_click_recipes = {entry.get("recipe") for entry in action_pools.get("doubleClick.random", {}).get("entries", [])}
    for recipe_id in ["doubleClick.holdIt", "doubleClick.objection", "doubleClick.eureka"]:
        recipe = recipes.get(recipe_id, {})
        require(recipe_id in double_click_recipes, f"doubleClick.random 缺少 {recipe_id}")
        require(recipe.get("sounds"), f"{recipe_id} 应声明多语言语音")
    require("doubleClick.takeThat" not in double_click_recipes, "看招丢徽章应由 Custom Interaction 概率触发")
    require("prosecutor_badge" in {
        item.get("id") if isinstance(item, dict) else item
        for item in manifest.get("customInteractions", [])
    }, "manifest 应声明检察官徽章 Custom Interaction")
    require(recipes.get("doubleClick.takeThat", {}).get("sounds"), "Take that recipe 应保留多语言语音定义")

    menu_tea_recipes = {entry.get("recipe") for entry in action_pools.get("menu.tea", {}).get("entries", [])}
    require({"tea.once", "teaAlt.once"} <= menu_tea_recipes, "菜单喝茶应随机覆盖旧版两组茶杯动作")
    require(recipes.get("sleep.enterLoopExit", {}).get("action") == "sleep", "睡觉 / 唤醒应走 sleep enter-loop-exit action")
    require(recipes.get("doubleClick.takeThat", {}).get("prop") == "prosecutor_badge", "Take that 应触发检察官徽章")
    require(manifest.get("props", {}).get("prosecutor_badge", {}).get("clickedRecipe") == "bow.once", "点击徽章应触发鞠躬")
    require(manifest.get("props", {}).get("prosecutor_badge", {}).get("expiredRecipe") == "pickup.once", "徽章自然消失应触发捡徽章")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    bridge_h = read("apps/desktop/src/pet/events/PetEventBridge.h")
    for token in [
        "consumeFrameMovementDelta",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少核心运行时入口 {token}")
    for token in [
        "submitPrimaryClick",
        "submitDoubleClick",
        "submitDragStarted",
        "submitDragMoved",
        "submitDragEnded",
        "submitPropClicked",
        "submitPropExpired",
        "submitMenuCommand",
        "submitIdleLoopFinished",
    ]:
        require(token in bridge_h, f"PetEventBridge.h 缺少核心事件入口 {token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    for token in [
        "submitIdleLoopFinished",
        "QSoundEffect",
        "m_propWindow",
        "m_singleClickTimer",
        "submitDoubleClick()",
        "submitDragMoved",
        "enabledSkinCommands",
        "submitMenuCommand(commandId)",
        'submitMenuCommand(QStringLiteral("runtime.sleep.toggle"))',
    ]:
        require(token in surface_cpp + surface_h + menu_cpp, f"原生表现层缺少核心入口 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_17_core_handfeel_coverage" in root_cmake, "CTest 未注册 Phase 0.17 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
