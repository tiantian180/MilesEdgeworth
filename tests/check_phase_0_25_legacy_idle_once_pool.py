#!/usr/bin/env python3
"""检查随机待机池覆盖旧版 once/0..19 的主要动作对。"""

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
    idle_entries = manifest.get("actionPools", {}).get("idle.random", {}).get("entries", [])
    idle_recipes = {entry.get("recipe") for entry in idle_entries}

    expected_actions = {
        "idle_sitting_tea": ["qrc:/pet/idle-sitting-tea-right.gif", "qrc:/pet/idle-sitting-tea-left.gif"],
        "idle_phone_call": ["qrc:/pet/idle-phone-call-right.gif", "qrc:/pet/idle-phone-call-left.gif"],
        "idle_look_back": ["qrc:/pet/idle-look-back-right.gif", "qrc:/pet/idle-look-back-left.gif"],
    }

    for action_id, urls in expected_actions.items():
        action = actions.get(action_id)
        require(action, f"manifest actions 缺少旧版随机待机动作 {action_id}")
        require(action.get("category") == "idle", f"{action_id} 应归类为 idle")
        require(action.get("loopMode") == "onceThenIdle", f"{action_id} 应播放一次后回到待机")
        variants = action.get("variants", {})
        require(variants.get("right", {}).get("animation") == urls[0], f"{action_id}.right 动画别名不正确")
        require(variants.get("left", {}).get("animation") == urls[1], f"{action_id}.left 动画别名不正确")

    expected_recipes = {
        "idle.sittingTea": "idle_sitting_tea",
        "idle.phoneCall": "idle_phone_call",
        "idle.lookBack": "idle_look_back",
    }

    for recipe_id, action_id in expected_recipes.items():
        recipe = recipes.get(recipe_id)
        require(recipe, f"recipes 缺少旧版随机待机 recipe {recipe_id}")
        require(recipe.get("scope") == "idle", f"{recipe_id} 应属于 idle scope")
        require(recipe.get("action") == action_id, f"{recipe_id} 应播放 {action_id}")
        require(recipe_id in idle_recipes, f"idle.random 缺少 {recipe_id}")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    for alias in [
        'alias="idle-sitting-tea-right.gif"',
        'alias="idle-sitting-tea-left.gif"',
        'alias="idle-phone-call-right.gif"',
        'alias="idle-phone-call-left.gif"',
        'alias="idle-look-back-right.gif"',
        'alias="idle-look-back-left.gif"',
    ]:
        require(alias in qrc, f"qrc 缺少 {alias}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_25_legacy_idle_once_pool" in root_cmake, "CTest 未注册 Phase 0.25 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
