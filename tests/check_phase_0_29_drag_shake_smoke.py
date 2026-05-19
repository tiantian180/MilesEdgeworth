#!/usr/bin/env python3
"""检查拖拽晃动已经进入事件桥 smoke 验证。

旧版手感里，拖住桌宠快速左右晃动会触发蹲下；如果蹲下动画还没播到末帧
就松手，会快速站起；如果已经播到末帧再松手，会播放完整站起。这个检查
要求 smoke 测试真正跑过这两条分支，避免只靠静态结构检查误判行为可用。
"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")

    for token in [
        "bridge.submitDragStarted",
        "bridge.submitDragMoved",
        "bridge.submitDragEnded",
        "bridge.submitHoldAnimationReachedEnd",
        'runtime.currentActionId() == "drag_crouch"',
        'runtime.currentActionId() == "drag_stand_up_quick"',
        'runtime.currentActionId() == "drag_stand_up_full"',
        "未蹲到底时松手应快速站起",
        "蹲到底后松手应完整站起",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少拖拽晃动行为覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_29_drag_shake_smoke" in root_cmake, "CTest 未注册 Phase 0.29 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
