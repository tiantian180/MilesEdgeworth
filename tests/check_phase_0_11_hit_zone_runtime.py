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
    "fallback",
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
    "click.fallback",
]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    manifest_path = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    skin_root = manifest_path.parent
    manifest_text = json.dumps(manifest, ensure_ascii=False)
    actions = manifest.get("actions", {})
    recipes = manifest.get("recipes", {})
    action_pools = manifest.get("animationPools", {})
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

    single_click = click_behaviors.get("singleClick", [])
    require([entry.get("pool") for entry in single_click] == CLICK_POOLS, "singleClick 应按旧版区域优先级映射到 click pools")
    require([entry.get("zone") for entry in single_click] == HIT_ZONES, "singleClick 应显式声明 zone 到 pool 的配对")

    for pool_id in CLICK_POOLS:
        require(pool_id in action_pools, f"animationPools 缺少 {pool_id}")
        entries = action_pools[pool_id].get("entries", [])
        require(entries, f"{pool_id} 至少需要一个候选动作")
        if pool_id == "click.fallback":
            require(entries == [{"command": "returnToIdle", "weight": 1}], "click.fallback 应只请求 returnToIdle")
        else:
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

    for url in [
        "file:assets/body/interaction/scared-right.gif",
        "file:assets/body/interaction/scared-left.gif",
        "file:assets/body/interaction/back-right.gif",
        "file:assets/body/interaction/back-left.gif",
        "file:assets/body/gestures/look-up-right.gif",
        "file:assets/body/gestures/look-up-left.gif",
        "file:assets/body/gestures/look-down-right.gif",
        "file:assets/body/gestures/look-down-left.gif",
    ]:
        require(url in manifest_text, f"manifest 缺少动画资源 {url}")
        require((skin_root / url[len("file:"):]).is_file(), f"皮肤文件缺失 {url}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    matcher_h = read("apps/desktop/src/pet/interaction/HitZoneMatcher.h")
    matcher_cpp = read("apps/desktop/src/pet/interaction/HitZoneMatcher.cpp")
    for token in [
        "SkinManifest m_manifest",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")
    for token in [
        "HitZoneDefinition",
        "hitZoneIdForPoint",
    ]:
        require(token in matcher_h, f"HitZoneMatcher.h 缺少 {token}")

    for token in [
        "PetEventType::PointerSingleClick",
        "HitZoneMatcher::hitZoneIdForPoint",
        "manifest.clickBehaviors.singleClick",
    ]:
        require(token in pipeline_cpp, f"InteractionPipeline.cpp 缺少 {token}")
    for token in [
        "manifest.hitZones",
        "candidateZoneIds",
    ]:
        require(token in matcher_cpp, f"HitZoneMatcher.cpp 缺少 {token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "m_dragMoved",
        "m_eventBridge->submitPrimaryClick(",
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow.cpp 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_11_hit_zone_runtime" in root_cmake, "CTest 未注册 Phase 0.11 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
