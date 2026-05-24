#!/usr/bin/env python3
"""检查 Phase 0.8 的 recipe / animationPool 动画调度骨架。

这一阶段开始把“播放一个素材”提升到“按意图触发一段动作编排”：
recipe 负责描述动作步骤，animationPool 负责让 idle、菜单、交互事件从候选动作中抽取。
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

    recipes = manifest.get("recipes", {})
    require(recipes, "manifest 缺少 recipes")
    for recipe_id in [
        "idle.stand",
        "idle.randomThinking",
        "objecting.once",
        "bow.once",
        "tea.once",
        "sleep.enterLoopExit",
    ]:
        require(recipe_id in recipes, f"manifest recipes 缺少 {recipe_id}")

    tea_recipe = recipes["tea.once"]
    require(tea_recipe.get("scope") == "menu", "tea.once 应声明为 menu recipe")
    require(tea_recipe.get("action") == "tea", "tea.once 应直接播放 tea GIF")
    require("steps" not in tea_recipe, "tea.once 不应额外串联 bow 或 idle_stand")

    action_pools = manifest.get("animationPools") or manifest.get("actionPools", {})
    require(action_pools, "manifest 缺少 animationPools/actionPools")
    for pool_id in ["idle.random", "menu.tea"]:
        require(pool_id in action_pools, f"manifest animationPools/actionPools 缺少 {pool_id}")

    idle_entries = action_pools["idle.random"].get("entries", [])
    require(len(idle_entries) >= 3, "idle.random 至少需要 3 个候选动作，才能模拟旧版随机待机")
    require(all("weight" in entry for entry in idle_entries), "idle.random 每个候选都应有 weight")
    require(any(entry.get("recipe") == "idle.randomThinking" for entry in idle_entries), "idle.random 应能抽到思考动作")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    selector_h = read("apps/desktop/src/pet/selection/AnimationPoolSelector.h")
    for token in [
        "Q_PROPERTY(QString currentRecipeId",
        "QString currentRecipeId() const",
        "Q_INVOKABLE void playRecipe",
        "Q_INVOKABLE void playAnimationFromPool",
        "currentRecipeChanged",
        "RecipeDefinition",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")
    require("AnimationPoolEntry" in manifest_h + selector_h, "AnimationPoolEntry 应由 SkinManifest / AnimationPoolSelector 承载")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    event_bridge_cpp = read("apps/desktop/src/pet/events/PetEventBridge.cpp")
    for token in [
        "m_manifest.recipes",
        "m_manifest.animationPools",
        "AnimationPoolSelector::selectEntry",
        "playNextRecipeStep",
        "clearActiveRecipe",
        "playAnimationFromPool",
        "currentRecipeChanged",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")
    require("#include <QRandomGenerator>" in event_bridge_cpp, "随机 idle 的随机数应由事件桥生成")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    for token in [
        "m_eventBridge->submitIdleLoopFinished()",
        "eventBridge->enabledSkinCommands()",
        "eventBridge->submitMenuCommand(commandId)",
    ]:
        require(token in surface_cpp + menu_cpp, f"原生表面 / 菜单缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_8_recipe_runtime" in root_cmake, "CTest 未注册 Phase 0.8 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
