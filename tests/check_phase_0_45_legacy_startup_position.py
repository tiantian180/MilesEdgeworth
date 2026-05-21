#!/usr/bin/env python3
"""检查 Phase 0.45 还原旧版启动入场位置。"""

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
    old_cpp = read_from_git("origin/legacy/v1-qt-widgets", "MilesEdgeworth.cpp")
    for token in [
        "BRIEFCASEIN",
        "desktopRect.x() - 45 * scale",
        "desktopRect.y() + desktopRect.height() - 90 * scale",
    ]:
        require(token in old_cpp, f"旧版启动定位缺少参考 token：{token}")

    shell_h = read("apps/desktop/src/DesktopShellController.h")
    for token in [
        "Q_INVOKABLE void placePetWindowForStartup",
        "QPointF legacyStartupPosition",
    ]:
        require(token in shell_h, f"DesktopShellController.h 缺少启动定位声明：{token}")

    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    for token in [
        "DesktopShellController::placePetWindowForStartup",
        "DesktopShellController::legacyStartupPosition",
        "QGuiApplication::primaryScreen",
        "availableGeometry()",
        "45.0 * safeScale",
        "90.0 * safeScale",
        "setPosition",
        "启动入场",
    ]:
        require(token in shell_cpp, f"DesktopShellController.cpp 缺少启动定位实现：{token}")

    main_cpp = read("apps/desktop/src/main.cpp")
    require("&petRuntime" in main_cpp, "main.cpp 的延迟初始化 lambda 应捕获 PetRuntime")
    require(
        "shellController.placePetWindowForStartup(petRuntime.petScale())" in main_cpp,
        "main.cpp 应在桌宠窗口绑定后应用旧版启动入场位置",
    )

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_45_legacy_startup_position" in root_cmake, "CTest 未注册 Phase 0.45 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
