#!/usr/bin/env python3
"""检查随机 idle 由站立循环完成事件触发。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    qml = read("apps/desktop/qml/PetWindow.qml")
    runtime_header = read("apps/desktop/src/pet/PetRuntime.h")
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")

    require("handleIdleLoopFinished" in runtime_header, "PetRuntime 应提供站立循环完成入口")
    require("handleIdleLoopFinishedForTest" in runtime_header, "PetRuntime 应提供可测的确定性站立循环入口")
    require("handleIdleLoopFinished" in qml, "QML 应在站立循环播完时通知 PetRuntime")
    require("idleRandomTimer" not in qml, "随机 idle 不应再依赖固定 7 秒 Timer")
    require("interval: 7000" not in qml, "随机 idle 不应保留旧的固定 7 秒触发间隔")
    require("handleIdleLoopFinishedForTest(0.69)" in smoke_test, "Smoke 应覆盖 70% 内触发随机 idle")
    require("handleIdleLoopFinishedForTest(0.71)" in smoke_test, "Smoke 应覆盖 70% 外继续站立")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_37_idle_loop_trigger" in root_cmake, "CTest 未注册 Phase 0.37 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
