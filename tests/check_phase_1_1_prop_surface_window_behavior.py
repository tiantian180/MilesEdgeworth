#!/usr/bin/env python3
"""Phase 1.1: Prop 窗口应复用桌宠窗口层级策略。

检察官徽章这类 Prop 虽然属于皮肤定制内容，但承载它的窗口是通用框架能力。
它必须和桌宠本体一样，不因 macOS 应用失焦而隐藏，并且跟随“始终置顶”
开关切换窗口层级。
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
    prop_h = read("apps/desktop/src/pet/surface/PropSurfaceWindow.h")
    prop_cpp = read("apps/desktop/src/pet/surface/PropSurfaceWindow.cpp")
    pet_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    root_cmake = read("CMakeLists.txt")

    for token in [
        "setAlwaysOnTop(bool alwaysOnTop)",
        "m_alwaysOnTop",
        "applyPlatformWindowBehavior",
    ]:
        require(token in prop_h + prop_cpp, f"PropSurfaceWindow 缺少层级同步入口：{token}")

    for token in [
        "platform/MacPetWindowBehavior.h",
        "Qt::WA_ShowWithoutActivating",
        "winId()",
        "applyMacPetWindowBaseBehavior(window)",
        "setMacPetWindowAlwaysOnTop(window, m_alwaysOnTop)",
    ]:
        require(token in prop_cpp, f"PropSurfaceWindow 未复用 macOS 桌宠窗口行为：{token}")

    for token in [
        "m_propWindow->setAlwaysOnTop(m_shellController->alwaysOnTop())",
        "DesktopShellController::alwaysOnTopChanged",
    ]:
        require(token in pet_cpp, f"PetSurfaceWindow 未把桌宠层级状态同步给 Prop：{token}")

    require(
        "check_phase_1_1_prop_surface_window_behavior" in root_cmake,
        "CTest 未注册 Phase 1.1 Prop 窗口行为检查",
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
