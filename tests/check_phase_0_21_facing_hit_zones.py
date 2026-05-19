#!/usr/bin/env python3
"""检查单击分区能随桌宠朝向切换。"""

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
    hit_zones = manifest.get("hitZones", {})

    for zone_id in ["face", "upper_arm", "forearm", "legs_back", "legs_look_down"]:
        variants = hit_zones.get(zone_id, {}).get("variants", {})
        require("right" in variants and "left" in variants, f"{zone_id} 应声明左右朝向点击区域")

    face = hit_zones["face"]["variants"]
    require(face["right"]["x"] > face["left"]["x"], "右朝向 face 区域应在画布右侧，左朝向 face 区域应在画布左侧")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    require("QHash<QString, QRectF> facingRects" in manifest_h, "HitZoneDefinition 应保存按朝向区分的 rect")
    require("rectForHitZone" in pet_runtime_h, "PetRuntime.h 应声明朝向感知 hit zone rect 选择器")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        'zoneObject.value("variants").toObject()',
        "zone.facingRects.insert",
    ]:
        require(token in loader_cpp, f"SkinManifestLoader.cpp 缺少朝向点击分区解析标记：{token}")
    for token in [
        "rectForHitZone(zone)",
        "zone.facingRects.value(m_currentFacing",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少朝向点击分区实现标记：{token}")

    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        "runtime.handlePrimaryClick(145, 40, 240, 240)",
        "右朝向点击脸部应触发 scared",
        "runtime.handlePrimaryClick(85, 40, 240, 240)",
        "左朝向点击脸部应触发 scared",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少朝向点击分区覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_21_facing_hit_zones" in root_cmake, "CTest 未注册 Phase 0.21 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
