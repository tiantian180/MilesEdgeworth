#!/usr/bin/env python3
"""检查 Phase 0.10 的 walk / run 最小移动驱动骨架。

旧版桌宠的走路和跑步是“动画帧驱动窗口移动”。v2 后续会升级成路线驱动，
但这一阶段先保留每帧移动增量，让 walk/run 在视觉上重新带动桌宠移动。
"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

MOVEMENT_DIRECTIONS = [
    "east",
    "west",
    "northEast",
    "northWest",
    "southEast",
    "southWest",
    "north",
    "south",
]


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

    require(manifest.get("movementDirections") == MOVEMENT_DIRECTIONS, "manifest 应显式声明旧版 8 方向移动顺序")

    for action_id in ["walk", "run"]:
        action = actions.get(action_id, {})
        require(action, f"manifest actions 缺少 {action_id}")
        require(action.get("category") == "locomotion", f"{action_id} 应归类为 locomotion")
        require(action.get("loopMode") == "onceThenIdle", f"{action_id} 当前应播放一轮后回 idle")

        variants = action.get("variants", {})
        require(set(variants.keys()) == set(MOVEMENT_DIRECTIONS), f"{action_id} 应包含 8 个移动方向 variant")
        for direction_id in MOVEMENT_DIRECTIONS:
            variant = variants[direction_id]
            require("animation" in variant, f"{action_id}.{direction_id} 缺少 animation")
            movement = variant.get("movement", {})
            require({"dx", "dy"} <= set(movement.keys()), f"{action_id}.{direction_id} 缺少每帧移动增量")

    require(actions["walk"]["variants"]["east"]["movement"]["dx"] > 0, "walk.east 应向右移动")
    require(actions["walk"]["variants"]["west"]["movement"]["dx"] < 0, "walk.west 应向左移动")
    require(actions["run"]["variants"]["south"]["movement"]["dy"] > actions["walk"]["variants"]["south"]["movement"]["dy"], "run 应比 walk 更快")
    require(actions["briefcase_in"]["variants"]["right"].get("movement", {}).get("dx", 0) > 0, "启动入场 briefcase_in 应推动窗口向右移动")

    for action_id in ["walk", "run"]:
        for direction_id in MOVEMENT_DIRECTIONS:
            recipe_id = f"{action_id}.{direction_id}"
            require(recipe_id in recipes, f"recipes 缺少 {recipe_id}")
            recipe = recipes[recipe_id]
            require(recipe.get("action") == action_id, f"{recipe_id} 应映射到 {action_id}")
            require(recipe.get("movementDirection") == direction_id, f"{recipe_id} 应声明 movementDirection")

    idle_entries = action_pools.get("idle.random", {}).get("entries", [])
    idle_recipe_ids = {entry.get("recipe") for entry in idle_entries}
    require(any(recipe_id.startswith("walk.") for recipe_id in idle_recipe_ids if recipe_id), "idle.random 应能抽到 walk")
    require(any(recipe_id.startswith("run.") for recipe_id in idle_recipe_ids if recipe_id), "idle.random 应能抽到 run")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    for action_id in ["walk", "run"]:
        for direction_id in MOVEMENT_DIRECTIONS:
            require(f'alias="{action_id}-{direction_id}.gif"' in qrc, f"qrc 缺少 {action_id}-{direction_id}.gif")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    for token in [
        "Q_PROPERTY(QString currentMovementDirection",
        "QString currentMovementDirection() const",
        "Q_INVOKABLE void playLocomotion",
        "Q_INVOKABLE QVariantMap consumeFrameMovementDelta",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")
    for token in ["movementDeltas", "movementDirection"]:
        require(token in manifest_h, f"SkinManifest.h 缺少移动字段 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "#include <QVariantMap>",
        "currentMovementDirectionChanged",
        "playLocomotion",
        "consumeFrameMovementDelta",
        "updateFacingFromMovementDirection",
        "movementDeltas",
        "movementDirection",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "m_runtime->consumeFrameMovementDelta()",
        "m_shellController->movePetWindowBy(dx, dy)",
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow.cpp 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_10_locomotion_runtime" in root_cmake, "CTest 未注册 Phase 0.10 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
