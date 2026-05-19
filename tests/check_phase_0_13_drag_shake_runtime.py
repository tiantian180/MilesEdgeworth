#!/usr/bin/env python3
"""检查 Phase 0.13 的拖拽晃动动作骨架。

旧版在左键拖拽时统计横向来回改变方向的次数：1 秒内达到 5 次后，
桌宠播放蹲下受惊动画；松手时根据蹲下动画是否已经播到末帧，选择完整
站起或快速恢复站起。这个检查先守住资源、manifest、运行时入口和 QML
事件连接，避免后续重构时把拖拽晃动链路拆散。
"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    actions = manifest.get("actions", {})

    expected_actions = {
        "drag_crouch": ("hold", ["qrc:/pet/crouch-right.gif", "qrc:/pet/crouch-left.gif"]),
        "drag_stand_up_full": ("onceThenIdle", ["qrc:/pet/stand-up-full-right.gif", "qrc:/pet/stand-up-full-left.gif"]),
        "drag_stand_up_quick": ("onceThenIdle", ["qrc:/pet/stand-up-quick-right.gif", "qrc:/pet/stand-up-quick-left.gif"]),
    }

    for action_id, (loop_mode, urls) in expected_actions.items():
        action = actions.get(action_id)
        require(action, f"manifest 缺少 {action_id}")
        require(action.get("category") == "interaction", f"{action_id} 应属于 interaction")
        require(action.get("loopMode") == loop_mode, f"{action_id} loopMode 应为 {loop_mode}")
        variants = action.get("variants", {})
        actual_urls = {variant.get("animation") for variant in variants.values()}
        for url in urls:
            require(url in actual_urls, f"{action_id} 缺少动画 {url}")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    for alias in [
        "crouch-right.gif",
        "crouch-left.gif",
        "stand-up-full-right.gif",
        "stand-up-full-left.gif",
        "stand-up-quick-right.gif",
        "stand-up-quick-left.gif",
    ]:
        require(alias in qrc, f"qrc 缺少 {alias}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "handleDragStarted",
        "handleDragMoved",
        "handleDragEnded",
        "handleHoldAnimationReachedEnd",
        "m_dragShakeClock",
        "m_dragShakeTurns",
        "m_dragHoldAnimationCompleted",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "drag_crouch",
        "drag_stand_up_full",
        "drag_stand_up_quick",
        "m_dragShakeClock.elapsed() > 1000",
        "m_dragShakeTurns >= 5",
        "m_dragHoldAnimationCompleted",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "m_runtime->handleDragStarted",
        "m_runtime->handleDragMoved",
        "m_runtime->handleDragEnded",
        "m_runtime->handleHoldAnimationReachedEnd",
        'm_runtime->currentLoopMode() == QStringLiteral("hold")',
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow.cpp 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_13_drag_shake_runtime" in root_cmake, "CTest 未注册 Phase 0.13 检查")


if __name__ == "__main__":
    main()
