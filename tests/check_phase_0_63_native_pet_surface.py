#!/usr/bin/env python3
"""Phase 0.63: 原生桌宠表面和逐帧 QWidget mask 检查。"""

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
        "apps/desktop/src/pet/surface/PetSurfaceWindow.h",
        "apps/desktop/src/pet/surface/PetSurfaceWindow.cpp",
    ]:
        require((ROOT / path).is_file(), f"缺少原生桌宠表面文件：{path}")

    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    main_cpp = read("apps/desktop/src/main.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")
    phase_record = read("docs/v2/阶段记录/第0阶段桌面壳验证.md")

    for token in [
        "class PetSurfaceWindow",
        "public QWidget",
        "PetRuntime",
        "PetEventBridge",
        "DesktopShellController",
        "QMovie",
        "QLabel",
    ]:
        require(token in surface_h + surface_cpp, f"PetSurfaceWindow 缺少 {token}")

    for token in [
        "applyCurrentFrameMask",
        "regionFromCurrentFrame",
        "currentPixmap",
        "QImage::Format_ARGB32",
        "qAlpha",
        "setMask(region)",
        "clearMask()",
        "frameChanged",
    ]:
        require(token in surface_cpp, f"原生桌宠表面缺少逐帧 mask 逻辑：{token}")

    for token in [
        "mousePressEvent",
        "mouseMoveEvent",
        "mouseReleaseEvent",
        "contextMenuEvent",
        "submitPrimaryClick",
        "submitDoubleClick",
        "handleDragStarted",
        "handleDragMoved",
        "handleDragEnded",
    ]:
        require(token in surface_h + surface_cpp, f"原生桌宠表面缺少鼠标交互迁移：{token}")

    for token in [
        "PetSurfaceWindow petSurfaceWindow",
        "petSurfaceWindow.show()",
        "shellController.setPetWindow(petSurfaceWindow.windowHandle())",
    ]:
        require(token in main_cpp, f"main.cpp 应使用原生桌宠表面：{token}")

    require(
        'engine.loadFromModule("MilesEdgeworth", "PetWindow")' not in main_cpp,
        "主程序不应继续把 QML Window 作为桌宠本体窗口",
    )

    for token in [
        "src/pet/surface/PetSurfaceWindow.cpp",
        "src/pet/surface/PetSurfaceWindow.h",
    ]:
        require(token in desktop_cmake, f"桌面 CMake 缺少原生桌宠表面：{token}")

    require("check_phase_0_63_native_pet_surface" in root_cmake, "CTest 未注册 Phase 0.63 检查")
    require("Phase 0.63" in phase_record, "阶段记录应登记 Phase 0.63")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
