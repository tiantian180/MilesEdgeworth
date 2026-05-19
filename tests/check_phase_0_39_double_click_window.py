#!/usr/bin/env python3
"""检查单击延迟确认窗口贴近旧版 300ms 双击判定。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    main_cpp = read_from_git("main", "MilesEdgeworth.cpp")

    require("clickTimer->setInterval(300)" in main_cpp, "旧版双击判定窗口应为 300ms")
    require("QTimer m_singleClickTimer" in surface_h, "原生表面应保留单击延迟确认 Timer")
    require("kDoubleClickIntervalMs = 300" in surface_cpp, "双击判定窗口常量应保持旧版 300ms")
    require(
        "m_singleClickTimer.setInterval(kDoubleClickIntervalMs)" in surface_cpp,
        "singleClickTimer 应使用 300ms 双击判定窗口常量",
    )
    require("m_singleClickTimer.stop()" in surface_cpp, "双击时应停止待执行的单击 Timer")
    require("m_eventBridge->submitDoubleClick()" in surface_cpp, "双击应进入 PetEventBridge")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_39_double_click_window" in root_cmake, "CTest 未注册 Phase 0.39 检查")
    return 0


def read_from_git(ref: str, path: str) -> str:
    import subprocess

    return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True)


if __name__ == "__main__":
    raise SystemExit(main())
