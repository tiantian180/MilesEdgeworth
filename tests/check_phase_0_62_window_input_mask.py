#!/usr/bin/env python3
"""Phase 0.62: 透明区域点击穿透输入 mask 检查。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    for path in [
        "apps/desktop/src/window/WindowInputMaskController.h",
        "apps/desktop/src/window/WindowInputMaskController.cpp",
    ]:
        require((ROOT / path).is_file(), f"缺少窗口输入 mask 文件：{path}")

    controller_h = read("apps/desktop/src/window/WindowInputMaskController.h")
    controller_cpp = read("apps/desktop/src/window/WindowInputMaskController.cpp")
    shell_h = read("apps/desktop/src/DesktopShellController.h")
    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    cmake = read("apps/desktop/CMakeLists.txt")

    for token in [
        "class WindowInputMaskController",
        "QRegion",
        "QImageReader",
        "regionFromImage",
        "applyMask",
    ]:
        require(token in controller_h + controller_cpp, f"WindowInputMaskController 缺少 {token}")

    require("setMask" in controller_cpp + surface_cpp, "输入 mask 应通过 Qt window/widget mask 应用")
    require("setPetInputMask" in shell_h + shell_cpp, "DesktopShellController 应暴露 setPetInputMask")
    require("clearPetInputMask" in shell_h + shell_cpp, "DesktopShellController 应暴露 clearPetInputMask")
    require("applyCurrentFrameMask" in surface_cpp, "原生表面应在动画或尺寸变化时更新输入 mask")
    require("regionFromCurrentFrame" in surface_cpp, "原生表面应从当前 GIF 帧生成输入 mask")
    require("WindowInputMaskController" in cmake, "CMake 应链接 WindowInputMaskController")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
