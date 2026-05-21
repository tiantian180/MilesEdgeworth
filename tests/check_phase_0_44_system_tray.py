#!/usr/bin/env python3
"""检查 v2 恢复旧版系统托盘入口。"""

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
        "createTrayIcon",
        "QSystemTrayIcon",
        "favicon_bar.ico",
        "on_trayActivated",
        "QSystemTrayIcon::Trigger",
    ]:
        require(token in old_cpp, f"旧版托盘行为缺少参考 token：{token}")

    shell_header = read("apps/desktop/src/DesktopShellController.h")
    for token in [
        "class QSystemTrayIcon",
        "class QMenu",
        "createTrayIcon",
        "revealPetWindow",
        "m_trayIcon",
        "m_trayMenu",
    ]:
        require(token in shell_header, f"DesktopShellController.h 缺少托盘声明：{token}")

    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    for token in [
        "QSystemTrayIcon",
        "QMenu",
        "QAction",
        "QIcon(\":/icon/favicon-bar.ico\")",
        "m_trayIcon->setContextMenu",
        "m_trayIcon->setToolTip",
        "QSystemTrayIcon::Trigger",
        "revealPetWindow",
        "QCoreApplication::quit",
    ]:
        require(token in shell_cpp, f"DesktopShellController.cpp 缺少托盘实现：{token}")

    main_cpp = read("apps/desktop/src/main.cpp")
    require("app.setQuitOnLastWindowClosed(false)" in main_cpp, "main.cpp 应禁用最后窗口关闭即退出，保留托盘常驻")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    require('alias="favicon-bar.ico"' in qrc, "qrc 应注册托盘图标 favicon-bar.ico")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_44_system_tray" in root_cmake, "CTest 未注册 Phase 0.44 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
