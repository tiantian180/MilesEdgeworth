#!/usr/bin/env python3
"""检查拖拽晃动识别已从 PetRuntime 抽到 GestureTracker。"""

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
    tracker_h_path = ROOT / "apps/desktop/src/pet/interaction/GestureTracker.h"
    tracker_cpp_path = ROOT / "apps/desktop/src/pet/interaction/GestureTracker.cpp"
    require(tracker_h_path.exists(), "缺少 GestureTracker.h")
    require(tracker_cpp_path.exists(), "缺少 GestureTracker.cpp")

    tracker_h = tracker_h_path.read_text(encoding="utf-8")
    tracker_cpp = tracker_cpp_path.read_text(encoding="utf-8")
    for token in [
        "class GestureTracker",
        "startDrag",
        "updateDrag",
        "finishDrag",
        "markHoldAnimationReachedEnd",
    ]:
        require(token in tracker_h + tracker_cpp, f"GestureTracker 缺少接口或实现标记：{token}")
    for token in [
        "QElapsedTimer",
        "elapsed() > kShakeWindowMs",
        "m_dragShakeTurns >= kShakeTurnThreshold",
        "m_dragHoldAnimationCompleted",
    ]:
        require(token in tracker_cpp + tracker_h, f"GestureTracker 缺少旧版晃动判定状态：{token}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "handleDragStarted",
        "handleDragMoved",
        "handleDragEnded",
        "handleHoldAnimationReachedEnd",
        "m_dragShakeClock",
        "m_dragShakeX",
        "m_dragShakeDirection",
        "m_dragShakeTurns",
        "m_dragShakeTracking",
        "m_dragHoldAnimationCompleted",
    ]:
        require(token not in pet_runtime_h + pet_runtime_cpp, f"PetRuntime 不应继续持有拖拽识别职责：{token}")
    for token in ["drag_crouch", "drag_stand_up_full", "drag_stand_up_quick"]:
        require(token not in pet_runtime_cpp, f"PetRuntime.cpp 不应出现 Miles 拖拽动作 id：{token}")

    pet_event_h = read("apps/desktop/src/pet/events/PetEvent.h")
    for token in [
        "PointerDragShake",
        "PointerDragReleased",
        "dragHoldCompleted",
        "pointerDragShake",
        "pointerDragReleased",
    ]:
        require(token in pet_event_h, f"PetEvent 缺少拖拽事件：{token}")

    bridge_h = read("apps/desktop/src/pet/events/PetEventBridge.h")
    bridge_cpp = read("apps/desktop/src/pet/events/PetEventBridge.cpp")
    for token in [
        '#include "pet/interaction/GestureTracker.h"',
        "GestureTracker m_gestureTracker",
        "submitDragStarted",
        "submitDragMoved",
        "submitDragEnded",
        "submitHoldAnimationReachedEnd",
    ]:
        require(token in bridge_h + bridge_cpp, f"PetEventBridge 缺少拖拽桥接：{token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "m_eventBridge->submitDragStarted",
        "m_eventBridge->submitDragMoved",
        "m_eventBridge->submitDragEnded",
        "m_eventBridge->submitHoldAnimationReachedEnd",
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow 应通过事件桥提交拖拽事件：{token}")

    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    for token in [
        "PetEventType::PointerDragShake",
        "PetEventType::PointerDragReleased",
        "manifest.behaviorRules",
    ]:
        require(token in pipeline_cpp, f"InteractionPipeline 缺少拖拽事件映射：{token}")
    for token in ["drag_crouch", "drag_stand_up_full", "drag_stand_up_quick"]:
        require(token not in pipeline_cpp, f"InteractionPipeline 不应硬编码 Miles 拖拽动作：{token}")

    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    rules = manifest.get("behaviorRules", [])
    require(any(rule.get("event") == "pointer.dragShake" and rule.get("action") == "drag_crouch" for rule in rules), "manifest 应声明 pointer.dragShake 行为")
    require(any(rule.get("event") == "pointer.dragReleased" and rule.get("when", {}).get("holdCompleted") is True for rule in rules), "manifest 应声明 hold 完成后的释放行为")
    require(any(rule.get("event") == "pointer.dragReleased" and rule.get("when", {}).get("holdCompleted") is False for rule in rules), "manifest 应声明 hold 未完成的释放行为")

    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    for token in [
        "src/pet/interaction/GestureTracker.cpp",
        "src/pet/interaction/GestureTracker.h",
    ]:
        require(desktop_cmake.count(token) >= 2, f"CMake 主程序和 smoke test 都应包含：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_67_gesture_tracker_extracted" in root_cmake, "CTest 未注册 Phase 0.67 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
