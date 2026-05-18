#!/usr/bin/env python3
"""检查 Phase 0.8 的 recipe / actionPool 动画调度骨架。

这一阶段开始把“播放一个素材”提升到“按意图触发一段动作编排”：
recipe 负责描述动作步骤，actionPool 负责让 idle、菜单、交互事件从候选动作中抽取。
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
        "tea.drinkThenBow",
        "sleep.enterLoopExit",
    ]:
        require(recipe_id in recipes, f"manifest recipes 缺少 {recipe_id}")

    tea_recipe = recipes["tea.drinkThenBow"]
    require(tea_recipe.get("scope") == "scenario", "tea.drinkThenBow 应声明为 scenario recipe")
    tea_steps = tea_recipe.get("steps", [])
    require([step.get("action") for step in tea_steps] == ["tea", "bow", "idle_stand"], "喝茶 recipe 应按 tea -> bow -> idle_stand 编排")

    action_pools = manifest.get("actionPools", {})
    require(action_pools, "manifest 缺少 actionPools")
    for pool_id in ["idle.random", "menu.tea", "ai.thinking"]:
        require(pool_id in action_pools, f"manifest actionPools 缺少 {pool_id}")

    idle_entries = action_pools["idle.random"].get("entries", [])
    require(len(idle_entries) >= 3, "idle.random 至少需要 3 个候选动作，才能模拟旧版随机待机")
    require(all("weight" in entry for entry in idle_entries), "idle.random 每个候选都应有 weight")
    require(any(entry.get("recipe") == "idle.randomThinking" for entry in idle_entries), "idle.random 应能抽到思考动作")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "Q_PROPERTY(QString currentRecipeId",
        "QString currentRecipeId() const",
        "Q_INVOKABLE void playRecipe",
        "Q_INVOKABLE void playActionFromPool",
        "Q_INVOKABLE void triggerIdle",
        "currentRecipeChanged",
        "RecipeDefinition",
        "ActionPoolEntry",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "#include <QRandomGenerator>",
        "m_recipes",
        "m_actionPools",
        "playNextRecipeStep",
        "clearActiveRecipe",
        "playActionFromPool",
        "triggerIdle",
        "currentRecipeChanged",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")

    pet_window_qml = read("apps/desktop/qml/PetWindow.qml")
    for token in [
        "idleRandomTimer",
        "App.PetRuntime.triggerIdle()",
        "App.PetRuntime.testTea()",
    ]:
        require(token in pet_window_qml, f"PetWindow.qml 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_8_recipe_runtime" in root_cmake, "CTest 未注册 Phase 0.8 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
