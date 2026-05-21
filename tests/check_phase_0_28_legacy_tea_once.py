#!/usr/bin/env python3
"""检查菜单喝茶按旧版只播放 tea GIF 本体。"""

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
    action_pools = manifest.get("actionPools", {})

    expected = {
        "tea.once": "tea",
        "teaAlt.once": "tea_alt",
    }

    for recipe_id, action_id in expected.items():
        recipe = recipes.get(recipe_id)
        require(recipe, f"recipes 缺少旧版喝茶 recipe {recipe_id}")
        require(recipe.get("scope") == "menu", f"{recipe_id} 应属于 menu scope")
        require(recipe.get("action") == action_id, f"{recipe_id} 应直接播放 {action_id}")
        require("steps" not in recipe, f"{recipe_id} 不应额外串联 bow 或 idle_stand")

    menu_entries = action_pools.get("menu.tea", {}).get("entries", [])
    menu_recipes = {entry.get("recipe") for entry in menu_entries}
    require(set(expected) <= menu_recipes, "menu.tea 应从两组旧版 tea GIF 中随机选择")
    require("tea.drinkThenBow" not in menu_recipes, "menu.tea 不应再引用额外串联鞠躬的旧 recipe")
    require("teaAlt.drinkThenBow" not in menu_recipes, "menu.tea 不应再引用额外串联鞠躬的旧 recipe")

    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        'runtime.currentRecipeId() == "tea.once"',
        'runtime.currentRecipeId() == "teaAlt.once"',
        "喝茶 recipe 应只播放茶杯 GIF 本体",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少旧版喝茶本体覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_28_legacy_tea_once" in root_cmake, "CTest 未注册 Phase 0.28 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
