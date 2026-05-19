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

    qml = read("apps/desktop/qml/PetWindow.qml")
    require("property bool doubleClickPending: false" in qml, "QML 应记录第二次点击待处理标志")
    require("if (singleClickTimer.running)" in qml, "第二次按下时应检测单击计时器是否仍在等待")
    require("doubleClickPending = true" in qml, "第二次按下时应进入双击待处理状态")
    require("singleClickTimer.stop()" in qml, "第二次按下时应取消第一次单击计时")

    release_branch = "if (doubleClickPending) {\n                doubleClickPending = false\n                App.PetRuntime.handleDoubleClick()\n                return\n            }"
    require(release_branch in qml, "第二次松手时应先处理双击并直接返回")
    require(
        qml.index("if (doubleClickPending)") < qml.index("singleClickTimer.restart()"),
        "双击分支必须早于单击计时器重启",
    )
    require("onDoubleClicked:" not in qml, "双击应由旧版 clickTimer 风格流程统一处理，避免 QML 双击信号和 release 分支重复触发")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_49_double_click_release_guard" in root_cmake, "CTest 未注册 Phase 0.49 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
