#!/usr/bin/env python3
"""检查肚子和腿部单击按旧版坐标分支拆成确定性子区域。"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def action_pool_recipes(manifest: dict, pool_id: str) -> list[str]:
    pool = manifest.get("actionPools", {}).get(pool_id, {})
    return [entry.get("recipe") for entry in pool.get("entries", [])]


def main() -> int:
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))

    click_order = manifest.get("clickBehaviors", {}).get("singleClick", [])
    expected = [
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
    require([entry.get("pool") for entry in click_order] == expected, "singleClick 应按旧版优先级包含确定性肚子/腿部子区域")

    for zone_id in ["belly_bow", "belly_pointing", "legs_back", "legs_look_down"]:
        require(zone_id in manifest.get("hitZones", {}), f"hitZones 缺少 {zone_id}")

    require(action_pool_recipes(manifest, "click.bellyBow") == ["bow.once"], "点击肚子上半应确定触发鞠躬")
    require(action_pool_recipes(manifest, "click.bellyPointingArea") == ["click.bellyPointing"], "点击肚子下半应确定触发指点")
    require(action_pool_recipes(manifest, "click.legsBackArea") == ["click.legsBack"], "点击靠当前朝向后侧腿部应确定触发后退")
    require(action_pool_recipes(manifest, "click.legsLookDownArea") == ["click.legsLookDown"], "点击另一侧腿部应确定触发低头看")

    matcher_cpp = read("apps/desktop/src/pet/interaction/HitZoneMatcher.cpp")
    require("hitZoneIdForClickPool" not in matcher_cpp, "HitZoneMatcher 不应继续写死 click pool 到 zone 的映射")
    for pool_id, zone_id in [
        ("click.bellyBow", "belly_bow"),
        ("click.bellyPointingArea", "belly_pointing"),
        ("click.legsBackArea", "legs_back"),
        ("click.legsLookDownArea", "legs_look_down"),
    ]:
        require({"pool": pool_id, "zone": zone_id} in click_order, f"manifest 缺少 {zone_id} -> {pool_id} 配对")

    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        "点击腰部上半应触发鞠躬而不是胸口动作",
        "点击肚子下半应触发指点",
        "右朝向点击左侧腿部应触发后退",
        "右朝向点击右侧腿部应触发低头看",
        "左朝向点击右侧腿部应触发后退",
        "左朝向点击左侧腿部应触发低头看",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少子区域覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_41_split_click_subzones" in root_cmake, "CTest 未注册 Phase 0.41 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
