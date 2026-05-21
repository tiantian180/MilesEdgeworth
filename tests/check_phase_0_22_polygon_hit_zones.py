#!/usr/bin/env python3
"""检查 hitZones 支持 polygon，用来还原旧版斜线点击区域。"""

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
    face = manifest.get("hitZones", {}).get("face", {})

    require(face.get("type") == "polygon", "face hit zone 应使用 polygon 还原旧版斜线区域")
    for facing in ["right", "left"]:
        polygon = face.get("variants", {}).get(facing, {}).get("polygon", [])
        require(len(polygon) >= 3, f"face.{facing} 应声明至少 3 个 polygon 点")
        require(all("x" in point and "y" in point for point in polygon), f"face.{facing} polygon 点应包含 x/y")

    matcher_h = read("apps/desktop/src/pet/interaction/HitZoneMatcher.h")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    for token in [
        "QList<QPointF> polygon",
        "QHash<QString, QList<QPointF>> facingPolygons",
    ]:
        require(token in manifest_h, f"SkinManifest.h 缺少 polygon hit zone 声明：{token}")

    for token in [
        "polygonForHitZone",
        "hitZoneContainsPoint",
    ]:
        require(token in matcher_h, f"HitZoneMatcher.h 缺少 polygon hit zone 声明：{token}")

    matcher_cpp = read("apps/desktop/src/pet/interaction/HitZoneMatcher.cpp")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    for token in [
        'zoneType != "rect" && zoneType != "polygon"',
        "polygonFromJsonArray",
        'zoneObject.value("polygon").toArray()',
        "zone.facingPolygons.insert",
    ]:
        require(token in loader_cpp, f"SkinManifestLoader.cpp 缺少 polygon hit zone 解析：{token}")

    for token in [
        "polygonForHitZone(zone, context)",
        "pointInPolygon",
        "hitZoneContainsPoint(zone, context, logicalPoint)",
    ]:
        require(token in matcher_cpp, f"HitZoneMatcher.cpp 缺少 polygon hit zone 实现：{token}")

    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        "bridge.submitPrimaryClick(90, 40, 240, 240)",
        "右朝向左上头部区域不应触发 scared",
        "bridge.submitPrimaryClick(150, 40, 240, 240)",
        "左朝向右上头部区域不应触发 scared",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少 polygon 边界覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_22_polygon_hit_zones" in root_cmake, "CTest 未注册 Phase 0.22 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
