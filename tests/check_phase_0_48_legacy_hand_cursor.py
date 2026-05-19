#!/usr/bin/env python3
"""检查 Phase 0.48 恢复旧版桌宠手型光标。"""

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
        "void MilesEdgeworth::enterEvent",
        "setCursor(Qt::PointingHandCursor)",
    ]:
        require(token in old_cpp, f"旧版手型光标缺少参考 token：{token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "setCursor(Qt::PointingHandCursor)",
        "mousePressEvent",
    ]:
        require(token in surface_cpp, f"PetSurfaceWindow.cpp 缺少手型光标入口：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_48_legacy_hand_cursor" in root_cmake, "CTest 未注册 Phase 0.48 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
