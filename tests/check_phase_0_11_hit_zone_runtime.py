#!/usr/bin/env python3
"""检查 Phase 0.11 的单击分区反应骨架。

旧版单击桌宠不同区域会触发不同动画。v2 先用 manifest 声明 hitZones
和 click actionPool，让普通换肤可以通过配置实现基础互动。
"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

HIT_ZONES = [
    "face",
    "head",
    "upper_arm",
    "forearm",
    "chest",
    "belly_bow",
    "belly_pointing",
    "legs_back",
    "legs_look_down",
]
CLICK_POOLS = [
    "click.face",
    "click.head",
    "click.upperArm",
    "click.forearm",
    "click.chest",
    "click.bellyBow",
    "click.bellyPointingArea",
    "click.legsBackArea",
    "click.legsLookDownArea",
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
    hit_zones = manifest.get("hitZones", {})
    click_behaviors = manifest.get("clickBehaviors", {})

    for action_id in ["scared", "back_away", "idle_look_up", "idle_look_down"]:
        require(action_id in actions, f"manifest actions 缺少 {action_id}")
        require(actions[action_id].get("loopMode") == "onceThenIdle", f"{action_id} 应播放一次后回 idle")

    require(hit_zones, "manifest 缺少 hitZones")
    for zone_id in HIT_ZONES:
        zone = hit_zones.get(zone_id, {})
        require(zone, f"hitZones 缺少 {zone_id}")
        require(zone.get("type") in {"rect", "polygon"}, f"{zone_id} 应使用已支持的 hit zone 类型")
        for key in ["x", "y", "width", "height"]:
            require(key in zone, f"{zone_id} 缺少 {key}")

    require(click_behaviors.get("singleClick") == CLICK_POOLS, "singleClick 应按旧版区域优先级映射到 click pools")

    for pool_id in CLICK_POOLS:
        require(pool_id in action_pools, f"actionPools 缺少 {pool_id}")
        entries = action_pools[pool_id].get("entries", [])
        require(entries, f"{pool_id} 至少需要一个候选动作")
        require(all("recipe" in entry for entry in entries), f"{pool_id} 候选应引用 recipe")

    expected_recipes = {
        "click.faceScared": "scared",
        "click.headTap": "idle_tapping_head",
        "click.headLookUp": "idle_look_up",
        "click.forearmWatch": "idle_check_watch",
        "click.forearmShrug": "idle_shrug",
        "click.chestThinking": "idle_thinking_once",
        "click.bellyPointing": "idle_pointing",
        "click.legsBack": "back_away",
        "click.legsLookDown": "idle_look_down",
    }
    for recipe_id, action_id in expected_recipes.items():
        require(recipes.get(recipe_id, {}).get("action") == action_id, f"{recipe_id} 应映射到 {action_id}")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    for alias in [
        "scared-right.gif",
        "scared-left.gif",
        "back-right.gif",
        "back-left.gif",
        "idle-look-up-right.gif",
        "idle-look-up-left.gif",
        "idle-look-down-right.gif",
        "idle-look-down-left.gif",
    ]:
        require(f'alias="{alias}"' in qrc, f"qrc 缺少 {alias}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    matcher_h = read("apps/desktop/src/pet/interaction/HitZoneMatcher.h")
    matcher_cpp = read("apps/desktop/src/pet/interaction/HitZoneMatcher.cpp")
    for token in [
        "Q_INVOKABLE void handlePrimaryClick",
        "SkinManifest m_manifest",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")
    for token in [
        "HitZoneDefinition",
        "clickPoolForPoint",
    ]:
        require(token in matcher_h, f"HitZoneMatcher.h 缺少 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "handlePrimaryClick",
        "HitZoneMatcher::clickPoolForPoint",
        "playActionFromPool(poolId)",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")
    for token in [
        "manifest.hitZones",
        "manifest.singleClickPools",
    ]:
        require(token in matcher_cpp, f"HitZoneMatcher.cpp 缺少 {token}")

    pet_window_qml = read("apps/desktop/qml/PetWindow.qml")
    for token in [
        "dragMoved",
        "App.PetRuntime.handlePrimaryClick(clickX, clickY, petWindow.width, petWindow.height)",
    ]:
        require(token in pet_window_qml, f"PetWindow.qml 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_11_hit_zone_runtime" in root_cmake, "CTest 未注册 Phase 0.11 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
