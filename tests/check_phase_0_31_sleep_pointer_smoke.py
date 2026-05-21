#!/usr/bin/env python3
"""检查睡眠中的鼠标交互已经进入 PetRuntime smoke 验证。

旧版睡眠循环中，单击桌宠不会打断睡眠；双击则等同于唤醒。
这个检查要求 smoke 测试覆盖这两个交互边界，避免后续把睡眠态误接入普通
单击分区或普通双击随机语音池。
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
        "睡眠中单击不应打断 sleep loop",
        "睡眠中双击应进入 wake/exit phase",
        "bridge.submitPrimaryClick(145, 40, 240, 240)",
        "bridge.submitDoubleClick()",
        'runtime.currentPhaseId() == "exit"',
        "wake 播完后应回到 idle_stand",
        "睡眠循环中 sleep toggle 事件应进入 wake/exit phase",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少睡眠鼠标交互覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_31_sleep_pointer_smoke" in root_cmake, "CTest 未注册 Phase 0.31 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
