#!/usr/bin/env python3
"""检查 Phase 0.47 恢复旧版双屏移动选项。"""

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
        "screenMenu = menu->addMenu(\"双屏选项\")",
        "singleScreen = screenMenu->addAction(\"单屏\")",
        "leftPrimary = screenMenu->addAction(\"主屏幕在左侧\")",
        "rightPrimary = screenMenu->addAction(\"主屏幕在右侧\")",
        "leftPrimary->isChecked()",
        "rightPrimary->isChecked()",
    ]:
        require(token in old_cpp, f"旧版双屏选项缺少参考 token：{token}")

    shell_h = read("apps/desktop/src/DesktopShellController.h")
    for token in [
        "Q_PROPERTY(QString screenLayoutMode",
        "Q_PROPERTY(int screenCount",
        "QString screenLayoutMode() const",
        "int screenCount() const",
        "void setScreenLayoutMode",
        "screenLayoutModeChanged",
        "screenCountChanged",
        "QRect virtualDesktopGeometry() const",
        "QString m_screenLayoutMode = \"single\"",
    ]:
        require(token in shell_h, f"DesktopShellController.h 缺少双屏声明：{token}")

    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    for token in [
        "DesktopShellController::screenLayoutMode",
        "DesktopShellController::screenCount",
        "DesktopShellController::setScreenLayoutMode",
        "DesktopShellController::virtualDesktopGeometry",
        "QGuiApplication::screens()",
        "screenAdded",
        "screenRemoved",
        "primaryLeft",
        "primaryRight",
        "m_screenLayoutMode != \"single\"",
        "virtualGeometry",
    ]:
        require(token in shell_cpp, f"DesktopShellController.cpp 缺少双屏实现：{token}")

    qml = read("apps/desktop/qml/PetWindow.qml")
    for token in [
        'title: "双屏选项"',
        'App.DesktopShell.setScreenLayoutMode("single")',
        'App.DesktopShell.setScreenLayoutMode("primaryLeft")',
        'App.DesktopShell.setScreenLayoutMode("primaryRight")',
        'checked: App.DesktopShell.screenLayoutMode === "single"',
        'enabled: App.DesktopShell.screenCount > 1',
    ]:
        require(token in qml, f"PetWindow.qml 缺少双屏菜单入口：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_47_legacy_dual_screen_options" in root_cmake, "CTest 未注册 Phase 0.47 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
