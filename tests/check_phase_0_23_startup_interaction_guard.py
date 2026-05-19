#!/usr/bin/env python3
"""检查启动入场期间的鼠标交互保护。"""

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
    require(
        manifest.get("actions", {}).get("briefcase_in", {}).get("blocksPointerInteraction") is True,
        "briefcase_in 应通过 manifest blocksPointerInteraction 禁用启动交互",
    )

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "Q_PROPERTY(bool pointerInteractionEnabled READ pointerInteractionEnabled NOTIFY pointerInteractionEnabledChanged)",
        "bool pointerInteractionEnabled() const",
        "bool acceptsPointerInteraction() const",
        "void pointerInteractionEnabledChanged()",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少启动交互保护声明：{token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "action.blocksPointerInteraction",
        "wasPointerInteractionEnabled",
        "pointerInteractionEnabledChanged",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少启动交互保护实现：{token}")

    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    bridge_cpp = read("apps/desktop/src/pet/events/PetEventBridge.cpp")
    for token in [
        "snapshot.pointerInteractionEnabled",
    ]:
        require(token in pipeline_cpp + bridge_cpp, f"事件层缺少启动交互保护判断：{token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "if (!m_runtime->pointerInteractionEnabled())",
        "showContextMenuAt",
        "m_shellController->movePetWindowBy(delta.x(), delta.y())",
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow.cpp 缺少启动交互保护入口：{token}")

    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        "require(!runtime.pointerInteractionEnabled()",
        "bridge.submitPrimaryClick(145, 40, 240, 240)",
        "启动入场期间单击不应打断 briefcase_in",
        "启动入场期间双击不应打断 briefcase_in",
        "require(runtime.pointerInteractionEnabled()",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少启动交互保护覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_23_startup_interaction_guard" in root_cmake, "CTest 未注册 Phase 0.23 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
