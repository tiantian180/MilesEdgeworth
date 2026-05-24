#!/usr/bin/env python3
"""检查抬头和低头动作也进入旧版随机待机池。"""

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
    idle_entries = manifest.get("animationPools", {}).get("idle.random", {}).get("entries", [])
    idle_recipes = {entry.get("recipe") for entry in idle_entries}

    expected_recipes = {
        "idle.lookDown": "idle_look_down",
        "idle.lookUp": "idle_look_up",
    }

    for recipe_id, action_id in expected_recipes.items():
        recipe = recipes.get(recipe_id)
        require(recipe, f"recipes 缺少旧版随机待机 recipe {recipe_id}")
        require(recipe.get("scope") == "idle", f"{recipe_id} 应属于 idle scope")
        require(recipe.get("action") == action_id, f"{recipe_id} 应播放 {action_id}")
        require(recipe_id in idle_recipes, f"idle.random 缺少 {recipe_id}")

    phase_25 = read("tests/check_phase_0_25_legacy_idle_once_pool.py")
    for action_id in ["idle_look_down", "idle_look_up"]:
        require(action_id in phase_25, f"Phase 0.25 检查应把 {action_id} 纳入 once/0..19 总表")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_26_idle_look_variants" in root_cmake, "CTest 未注册 Phase 0.26 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
