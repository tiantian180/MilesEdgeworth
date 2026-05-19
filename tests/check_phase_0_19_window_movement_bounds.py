#!/usr/bin/env python3
"""检查 Phase 0.19 的桌宠窗口移动边界入口。

旧版在 moveEvent 中限制窗口位置，避免自动走路/跑步把桌宠移出屏幕。
v2 将这个能力收口到 DesktopShellController，QML 的动画驱动移动和拖拽移动
都应走同一个壳层入口。
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
    shell_h = read("apps/desktop/src/DesktopShellController.h")
    for token in [
        "Q_INVOKABLE void movePetWindowBy",
        "Q_INVOKABLE void movePetWindowTo",
        "QPointF clampedPetWindowPosition",
    ]:
        require(token in shell_h, f"DesktopShellController.h 缺少 {token}")

    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    for token in [
        "#include <QGuiApplication>",
        "#include <QScreen>",
        "DesktopShellController::movePetWindowBy",
        "DesktopShellController::movePetWindowTo",
        "DesktopShellController::clampedPetWindowPosition",
        "legacyBodyCenter",
        "qBound",
    ]:
        require(token in shell_cpp, f"DesktopShellController.cpp 缺少 {token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "m_shellController->movePetWindowBy(dx, dy)",
        "m_shellController->movePetWindowBy(delta.x(), delta.y())",
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow.cpp 缺少 {token}")

    require("move(" not in surface_cpp, "自动移动不应绕过 DesktopShellController 直接移动窗口")

    runtime_smoke = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    require("walk.east 应推动窗口向右移动" in runtime_smoke, "运行时 smoke 应继续覆盖 walk.east 移动增量")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_19_window_movement_bounds" in root_cmake, "CTest 未注册 Phase 0.19 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
