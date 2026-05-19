#!/usr/bin/env python3
"""检查 Phase 0.57 的 hit zone 命中逻辑已经从 PetRuntime 拆出。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    matcher_h_path = ROOT / "apps/desktop/src/pet/interaction/HitZoneMatcher.h"
    matcher_cpp_path = ROOT / "apps/desktop/src/pet/interaction/HitZoneMatcher.cpp"
    require(matcher_h_path.exists(), "缺少 HitZoneMatcher.h")
    require(matcher_cpp_path.exists(), "缺少 HitZoneMatcher.cpp")

    matcher_h = matcher_h_path.read_text(encoding="utf-8")
    matcher_cpp = matcher_cpp_path.read_text(encoding="utf-8")
    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")

    for token in [
        "struct HitZoneMatchContext",
        "class HitZoneMatcher",
        "static QString clickPoolForPoint",
        "static QRectF rectForHitZone",
        "static QList<QPointF> polygonForHitZone",
        "static bool hitZoneContainsPoint",
    ]:
        require(token in matcher_h, f"HitZoneMatcher.h 缺少接口：{token}")

    for token in [
        "hitZoneIdForClickPool",
        "pointInPolygon",
        "context.canvasWidth",
        "context.canvasHeight",
        "manifest.singleClickPools",
        "manifest.hitZones",
    ]:
        require(token in matcher_cpp, f"HitZoneMatcher.cpp 缺少实现标记：{token}")

    for removed_token in [
        "QRectF rectForHitZone",
        "QList<QPointF> polygonForHitZone",
        "bool hitZoneContainsPoint",
        "QString clickPoolForPoint",
    ]:
        require(removed_token not in pet_runtime_h, f"PetRuntime.h 不应继续声明：{removed_token}")

    for token in [
        '#include "pet/interaction/HitZoneMatcher.h"',
        "HitZoneMatchContext hitZoneContext",
        "HitZoneMatcher::clickPoolForPoint",
        "playActionFromPool(poolId)",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少接入标记：{token}")

    for token in [
        "src/pet/interaction/HitZoneMatcher.cpp",
        "src/pet/interaction/HitZoneMatcher.h",
    ]:
        require(desktop_cmake.count(token) >= 2, f"CMake 主程序和 smoke test 都应包含：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_57_hit_zone_matcher" in root_cmake, "CTest 未注册 Phase 0.57 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
