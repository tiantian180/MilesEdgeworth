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
    qml = read("apps/desktop/qml/PetWindow.qml")
    main_cpp = read_from_git("main", "MilesEdgeworth.cpp")

    require("clickTimer->setInterval(300)" in main_cpp, "旧版双击判定窗口应为 300ms")
    require("id: singleClickTimer" in qml, "QML 应保留单击延迟确认 Timer")
    require("interval: 300" in qml, "singleClickTimer 应使用旧版 300ms 双击判定窗口")
    require("singleClickTimer.stop()" in qml, "双击时应停止待执行的单击 Timer")
    require("App.PetEventBridge.submitDoubleClick()" in qml, "双击应进入 PetRuntime")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_39_double_click_window" in root_cmake, "CTest 未注册 Phase 0.39 检查")
    return 0


def read_from_git(ref: str, path: str) -> str:
    import subprocess

    return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True)


if __name__ == "__main__":
    raise SystemExit(main())
