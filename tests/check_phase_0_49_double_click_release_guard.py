#!/usr/bin/env python3
"""检查双击不会在第二次松手后补触发单击事件。"""

from __future__ import annotations

import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def read_from_git(ref: str, path: str) -> str:
    return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    old_cpp = read_from_git("main", "MilesEdgeworth.cpp")
    for token in [
        "doubleClick = true",
        "clickTimer->stop()",
        "doubleClickEvent()",
        "clickTimer->start()",
    ]:
        require(token in old_cpp, f"旧版单双击判定缺少参考 token：{token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    require("bool m_doubleClickPending = false" in surface_h, "原生表面应记录第二次点击待处理标志")
    require("if (m_singleClickTimer.isActive())" in surface_cpp, "第二次按下时应检测单击计时器是否仍在等待")
    require("m_doubleClickPending = true" in surface_cpp, "第二次按下时应进入双击待处理状态")
    require("m_singleClickTimer.stop()" in surface_cpp, "第二次按下时应取消第一次单击计时")

    release_branch = "if (m_doubleClickPending) {\n        m_doubleClickPending = false;\n        m_eventBridge->submitDoubleClick();\n        return;\n    }"
    require(release_branch in surface_cpp, "第二次松手时应先处理双击并直接返回")
    require(
        surface_cpp.index("if (m_doubleClickPending)") < surface_cpp.index("m_singleClickTimer.start()"),
        "双击分支必须早于单击计时器重启",
    )
    require("mouseDoubleClickEvent" not in surface_cpp + surface_h, "双击应由旧版 clickTimer 风格流程统一处理，避免系统双击事件和 release 分支重复触发")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_49_double_click_release_guard" in root_cmake, "CTest 未注册 Phase 0.49 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
