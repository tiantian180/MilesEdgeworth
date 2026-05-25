#!/usr/bin/env python3
"""检查走路 / 跑步结束后的续行动作骨架。"""

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
    action_pools = manifest.get("animationPools", {})
    behavior_triggers = manifest.get("behaviorTriggers", {})

    # 旧版 walk / run 播完后不会总是立刻站住，而是有概率继续移动。
    # v2 用 follow-up pool 表达这件事，后续换肤时可以按皮肤喜好调整权重。
    for recipe_id, action_id in [("walk.current", "walk"), ("run.current", "run")]:
        recipe = recipes.get(recipe_id, {})
        require(recipe.get("action") == action_id, f"{recipe_id} 应播放 {action_id}")
        require(recipe.get("movementDirection") == "$current", f"{recipe_id} 应沿用当前移动方向")

    walk_finished_recipes = {entry.get("recipe") for entry in action_pools.get("walk.finished", {}).get("entries", [])}
    require("idle.stand" in walk_finished_recipes, "walk.finished 应允许停下回到站立")
    require("run.current" in walk_finished_recipes, "walk.finished 应允许沿当前方向切到跑步")
    require("walk.current" in walk_finished_recipes, "walk.finished 应允许沿当前方向继续走")
    require(any(recipe_id and recipe_id.startswith("walk.") and recipe_id != "walk.current" for recipe_id in walk_finished_recipes), "walk.finished 应允许随机换一个走路方向")

    run_finished_recipes = {entry.get("recipe") for entry in action_pools.get("run.finished", {}).get("entries", [])}
    require("idle.stand" in run_finished_recipes, "run.finished 应允许停下回到站立")
    require("walk.current" in run_finished_recipes, "run.finished 应允许沿当前方向切到走路")
    require("run.current" in run_finished_recipes, "run.finished 应允许沿当前方向继续跑")
    require(any(recipe_id and recipe_id.startswith("run.") and recipe_id != "run.current" for recipe_id in run_finished_recipes), "run.finished 应允许随机换一个跑步方向")

    completed_entries = behavior_triggers.get("action.completed", {}).get("entries", [])
    require(
        any(entry.get("when", {}).get("action") == "walk" and entry.get("pool") == "walk.finished" for entry in completed_entries),
        "action.completed 应把 walk 完成映射到 walk.finished pool",
    )
    require(
        any(entry.get("when", {}).get("action") == "run" and entry.get("pool") == "run.finished" for entry in completed_entries),
        "action.completed 应把 run 完成映射到 run.finished pool",
    )

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    require("followUpPoolForCompletedAction" not in pet_runtime_h, "动作完成后的续接选择不应继续留在 PetRuntime")
    require("resolveRecipeMovementDirection" in pet_runtime_h, "PetRuntime.h 应声明 recipe 移动方向解析器")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "PetEvent::actionCompleted",
        "submitRuntimeEvent",
        'normalizedDirection == "$current"',
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少续行实现标记：{token}")
    for token in ['return "walk.finished"', 'return "run.finished"']:
        require(token not in pet_runtime_cpp, f"PetRuntime.cpp 不应硬编码续行动作池：{token}")

    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        'runtime.playRecipe("run.current")',
        'runtime.currentMovementDirection() == "northEast"',
        'runtime.playRecipe("walk.current")',
        "walk/run current recipe 应沿用当前方向",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少当前方向续行覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_20_locomotion_followup" in root_cmake, "CTest 未注册 Phase 0.20 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
