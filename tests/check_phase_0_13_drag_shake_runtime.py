#!/usr/bin/env python3
"""检查 Phase 0.13 的拖拽晃动动作骨架。

旧版在左键拖拽时统计横向来回改变方向的次数：1 秒内达到 5 次后，
桌宠播放蹲下受惊动画；松手时根据蹲下动画是否已经播到末帧，选择完整
站起或快速恢复站起。这个检查先守住资源、manifest、事件桥和原生 surface
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
    skin_root = ROOT / "apps/desktop/resources/skins/miles-edgeworth"
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    actions = manifest.get("actions", {})
    behavior_rules = manifest.get("behaviorRules", [])

    expected_actions = {
        "drag_crouch": ("hold", ["file:assets/body/interaction/crouch-right.gif", "file:assets/body/interaction/crouch-left.gif"]),
        "drag_stand_up_full": ("onceThenIdle", ["file:assets/body/interaction/stand-up-full-right.gif", "file:assets/body/interaction/stand-up-full-left.gif"]),
        "drag_stand_up_quick": ("onceThenIdle", ["file:assets/body/interaction/stand-up-quick-right.gif", "file:assets/body/interaction/stand-up-quick-left.gif"]),
    }

    for action_id, (loop_mode, urls) in expected_actions.items():
        action = actions.get(action_id)
        require(action, f"manifest 缺少 {action_id}")
        require(action.get("category") == "interaction", f"{action_id} 应属于 interaction")
        require(action.get("loopMode") == loop_mode, f"{action_id} loopMode 应为 {loop_mode}")
        variants = action.get("variants", {})
        actual_urls = {variant.get("clip") for variant in variants.values()}
        for url in urls:
            require(url in actual_urls, f"{action_id} 缺少动画 {url}")
            require((skin_root / url[len("file:"):]).is_file(), f"皮肤文件缺失 {url}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "handleDragStarted",
        "handleDragMoved",
        "handleDragEnded",
        "handleHoldAnimationReachedEnd",
        "m_dragShakeClock",
        "m_dragShakeTurns",
        "m_dragHoldAnimationCompleted",
    ]:
        require(token not in pet_runtime_h + pet_runtime_cpp, f"PetRuntime 不应继续持有拖拽识别职责：{token}")

    gesture_tracker = read("apps/desktop/src/pet/interaction/GestureTracker.cpp")
    for token in [
        "m_dragShakeClock.elapsed() > kShakeWindowMs",
        "m_dragShakeTurns >= kShakeTurnThreshold",
        "m_dragHoldAnimationCompleted",
    ]:
        require(token in gesture_tracker, f"GestureTracker.cpp 缺少 {token}")

    require(any(rule.get("event") == "pointer.dragShake" and rule.get("action") == "drag_crouch" for rule in behavior_rules), "behaviorRules 缺少 pointer.dragShake -> drag_crouch")
    require(any(rule.get("event") == "pointer.dragReleased" and rule.get("action") == "drag_stand_up_full" for rule in behavior_rules), "behaviorRules 缺少完整站起分支")
    require(any(rule.get("event") == "pointer.dragReleased" and rule.get("action") == "drag_stand_up_quick" for rule in behavior_rules), "behaviorRules 缺少快速站起分支")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "m_eventBridge->submitDragStarted",
        "m_eventBridge->submitDragMoved",
        "m_eventBridge->submitDragEnded",
        "m_eventBridge->submitHoldAnimationReachedEnd",
        'm_runtime->currentLoopMode() == QStringLiteral("hold")',
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow.cpp 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_13_drag_shake_runtime" in root_cmake, "CTest 未注册 Phase 0.13 检查")


if __name__ == "__main__":
    main()
