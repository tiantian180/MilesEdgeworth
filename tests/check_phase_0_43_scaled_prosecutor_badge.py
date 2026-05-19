#!/usr/bin/env python3
"""检查检察官徽章会随旧版尺寸档位一起缩放飞出。"""

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
    badge = manifest.get("props", {}).get("prosecutor_badge", {})
    require(badge, "manifest 缺少 prosecutor_badge")

    for field in ["visualWidth", "visualHeight", "travelBase", "travelPerScale"]:
        require(field in badge, f"prosecutor_badge 缺少缩放字段 {field}")

    require(badge.get("visualWidth") == 70, "prosecutor_badge visualWidth 应保留原始 PNG 视觉宽度")
    require(badge.get("visualHeight") == 70, "prosecutor_badge visualHeight 应保留原始 PNG 视觉高度")
    require(badge["travelBase"]["right"]["x"] == 600, "右向徽章基础飞行距离应来自旧版 600")
    require(badge["travelPerScale"]["right"]["x"] == 150, "右向徽章每 scale 飞行补偿应来自旧版 150")
    require(badge["travelBase"]["left"]["x"] == -600, "左向徽章基础飞行距离应来自旧版 -600")
    require(badge["travelPerScale"]["left"]["x"] == -150, "左向徽章每 scale 飞行补偿应来自旧版 -150")

    runtime_header = read("apps/desktop/src/pet/PetRuntime.h")
    manifest_header = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    for token in [
        "currentPropVisualWidth READ currentPropVisualWidth",
        "currentPropVisualHeight READ currentPropVisualHeight",
    ]:
        require(token in runtime_header, f"PetRuntime.h 缺少 Prop 缩放入口：{token}")

    for token in [
        "travelBaseDeltas",
        "travelPerScaleDeltas",
    ]:
        require(token in manifest_header, f"SkinManifest.h 缺少 Prop 缩放字段：{token}")

    prop_controller_cpp = read("apps/desktop/src/pet/effects/PropController.cpp")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    for token in [
        "scaledPoint",
        "travelDelta",
        "prop.visualWidth",
        "prop.visualHeight",
    ]:
        require(token in prop_controller_cpp, f"PropController.cpp 缺少 Prop 缩放逻辑：{token}")
    for token in [
        "travelBase",
        "travelPerScale",
    ]:
        require(token in loader_cpp, f"SkinManifestLoader.cpp 缺少 Prop 缩放解析：{token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "m_runtime->currentPropVisualWidth()",
        "m_runtime->currentPropVisualHeight()",
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow.cpp 缺少 Prop 视觉尺寸绑定：{token}")

    smoke = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        "大号徽章右向起点应按旧版 scale=3 缩放",
        "大号徽章右向终点应按旧版 600 + 150 * scale 计算",
        "迷你徽章左向起点应按旧版 scale=1 缩放",
        "迷你徽章左向终点应按旧版 -(600 + 150 * scale) 计算",
    ]:
        require(token in smoke, f"PetRuntimeSmoke 缺少徽章缩放覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_43_scaled_prosecutor_badge" in root_cmake, "CTest 未注册 Phase 0.43 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
