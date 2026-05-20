#!/usr/bin/env python3
"""Phase 1.2: 移动动画启动时不能跳过首帧时长。

Qt QMovie 在 `jumpToFrame(0)` 后再 `start()` 时，会先同步触发 frame 0，
随后 `start()` 立刻触发 frame 1。walk / run 只有 6 帧，这会让首帧没有
正常显示时长，也会让窗口移动在同一时刻执行两次，体感像少播一帧。
"""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def extract_function(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^)]*\)\s*\{{", source)
    if match is None:
        raise AssertionError(f"找不到函数：{name}")

    start = match.start()
    index = match.end() - 1
    depth = 0
    while index < len(source):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
        index += 1

    raise AssertionError(f"函数括号没有闭合：{name}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def strip_cpp_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    source = re.sub(r"//.*", "", source)
    return source


def main() -> int:
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    restart_body = extract_function(surface_cpp, "PetSurfaceWindow::restartMovieFromRuntime")
    restart_code = strip_cpp_comments(restart_body)
    root_cmake = read("CMakeLists.txt")

    require("m_movie->start();" in restart_code, "切换动画仍应通过 QMovie::start() 从首帧开始播放")
    require(
        "jumpToFrame(0)" not in restart_code,
        "切换动画不能先 jumpToFrame(0) 再 start()，否则 walk/run 首帧会被立即跳过",
    )
    require(
        "check_phase_1_2_locomotion_qmovie_start" in root_cmake,
        "CTest 未注册 Phase 1.2 移动动画首帧检查",
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
