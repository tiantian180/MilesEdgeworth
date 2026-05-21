#!/usr/bin/env python3
"""检查 8 方向 walk/run 已进入 PetRuntime smoke 验证。

旧版走路和跑步都有 8 个方向，并且每一帧都会推动窗口移动。这个检查要求
smoke 测试真实播放所有 `walk.*` / `run.*` recipe，验证移动方向、移动
增量符号、朝向和跑步速度，避免只靠 manifest 静态结构检查。
"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

MOVEMENT_DIRECTIONS = [
    "east",
    "west",
    "northEast",
    "northWest",
    "southEast",
    "southWest",
    "north",
    "south",
]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")

    for token in [
        "MovementCase",
        "requireSignedDelta",
        'QStringLiteral("walk.%1")',
        'QStringLiteral("run.%1")',
        "runSpeed > walkSpeed",
        "run 应比 walk 移动更快",
        "移动方向应更新到 recipe 声明的方向",
        "移动方向应更新桌宠朝向",
        "纯纵向移动应保留原朝向",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少 8 方向移动覆盖：{token}")

    for direction in MOVEMENT_DIRECTIONS:
        require(f'"{direction}"' in smoke_test, f"PetRuntimeSmoke 缺少移动方向 {direction}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_34_locomotion_direction_smoke" in root_cmake, "CTest 未注册 Phase 0.34 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
