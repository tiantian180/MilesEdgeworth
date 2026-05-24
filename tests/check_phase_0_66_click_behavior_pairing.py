#!/usr/bin/env python3
"""检查单击 / 双击行为映射已从 C++ 硬编码迁入 manifest。"""

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
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    click_behaviors = manifest.get("clickBehaviors", {})
    hit_zones = manifest.get("hitZones", {})
    action_pools = manifest.get("animationPools", {})

    single_click = click_behaviors.get("singleClick")
    require(isinstance(single_click, list) and single_click, "clickBehaviors.singleClick 应是非空列表")
    for index, entry in enumerate(single_click):
        require(isinstance(entry, dict), f"singleClick[{index}] 应使用 {{zone, pool}} 对象")
        zone_id = entry.get("zone")
        pool_id = entry.get("pool")
        require(zone_id in hit_zones, f"singleClick[{index}] zone 未在 hitZones 中声明: {zone_id!r}")
        require(pool_id in action_pools, f"singleClick[{index}] pool 未在 animationPools 中声明: {pool_id!r}")

    double_click = click_behaviors.get("doubleClick")
    require(isinstance(double_click, list) and double_click, "clickBehaviors.doubleClick 应是非空列表")
    for index, entry in enumerate(double_click):
        require(isinstance(entry, dict), f"doubleClick[{index}] 应使用对象声明")
        when = entry.get("when", "default")
        require(when == "default", f"Phase 0.66 暂只支持 doubleClick when=default: {when!r}")
        request_targets = [entry.get("pool"), entry.get("recipe"), entry.get("action"), entry.get("customInteraction")]
        require(any(request_targets), f"doubleClick[{index}] 应声明 pool / recipe / action / customInteraction")
        if entry.get("pool"):
            require(entry["pool"] in action_pools, f"doubleClick[{index}] pool 未在 animationPools 中声明")

    matcher_cpp = read("apps/desktop/src/pet/interaction/HitZoneMatcher.cpp")
    matcher_h = read("apps/desktop/src/pet/interaction/HitZoneMatcher.h")
    require("hitZoneIdForClickPool" not in matcher_cpp, "HitZoneMatcher 不应保留 click pool 到 hit zone 的硬编码映射")
    require("click." not in matcher_cpp, "HitZoneMatcher 不应出现 click.* 皮肤池 id")
    require("clickPoolForPoint" not in matcher_cpp + matcher_h, "HitZoneMatcher 不应直接返回 click pool")
    require("hitZoneIdForPoint" in matcher_cpp + matcher_h, "HitZoneMatcher 应只返回命中的 hit zone id")

    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    require('"doubleClick.random"' not in pipeline_cpp, "双击默认池应从 manifest clickBehaviors.doubleClick 读取")
    require("clickBehaviors.singleClick" in pipeline_cpp, "单击 zone 到请求的映射应由 InteractionPipeline 读取")
    require("clickBehaviors.doubleClick" in pipeline_cpp, "双击默认请求应由 InteractionPipeline 读取")

    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    require("ClickBehaviorEntry" in manifest_h, "SkinManifest 应声明 ClickBehaviorEntry")
    require("ClickBehaviorDefinition" in manifest_h, "SkinManifest 应声明 ClickBehaviorDefinition")
    require("QStringList singleClickPools" not in manifest_h, "SkinManifest 不应继续暴露 singleClickPools 旧字段")

    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    require('value("zone")' in loader_cpp, "SkinManifestLoader 应解析 singleClick zone")
    require('value("pool")' in loader_cpp, "SkinManifestLoader 应解析 click behavior pool")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_66_click_behavior_pairing" in root_cmake, "CTest 未注册 Phase 0.66 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
