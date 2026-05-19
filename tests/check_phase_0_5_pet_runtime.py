#!/usr/bin/env python3
"""检查 Phase 0.5 的 Pet Runtime 最小纵切是否接线完整。

这里做的是轻量静态检查，不替代真实 UI 验证。它的目的主要是防止后续重构时
不小心退回到表现层写死 GIF、或者退回到 context property 注入对象的旧写法。
"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    main_cpp = read("apps/desktop/src/main.cpp")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    qrc = read("apps/desktop/resources/pet_assets.qrc")

    require('setContextProperty("desktopShell"' not in main_cpp, "desktopShell 不应再通过 context property 注入")
    require("PetRuntime petRuntime" in main_cpp, "main.cpp 应创建 PetRuntime 实例")
    require(
        "DesktopShellControllerForeign::s_instance = &shellController" in main_cpp,
        "DesktopShell singleton 没有绑定现有 C++ 实例",
    )
    require(
        "PetRuntimeForeign::s_instance = &petRuntime" in main_cpp,
        "PetRuntime singleton 没有绑定现有 C++ 实例",
    )

    require("PetSurfaceWindow petSurfaceWindow" in main_cpp, "main.cpp 应使用原生桌宠表面")
    require("PetEventBridge petEventBridge" in main_cpp, "main.cpp 应创建 PetEventBridge")
    require("shellController->alwaysOnTop()" in menu_cpp, "菜单应读取 DesktopShellController.alwaysOnTop")
    require("DesktopShellController::toggleAlwaysOnTop" in menu_cpp, "菜单应调用 DesktopShellController.toggleAlwaysOnTop")
    require("m_runtime->currentAnimationUrl()" in surface_cpp, "动画源应绑定 PetRuntime.currentAnimationUrl")
    require('"qrc:/pet/stand-right.gif"' not in surface_cpp + menu_cpp, "表现层不应直接写死待机 GIF")

    require("src/pet/PetRuntime.cpp" in desktop_cmake, "PetRuntime.cpp 应加入桌面目标")
    require("src/pet/PetRuntime.h" in desktop_cmake, "PetRuntime.h 应加入 QML module sources")

    for alias in ["thinking-right.gif", "objecting-right.gif", "manifest.json"]:
        require(f'alias="{alias}"' in qrc, f"qrc 缺少 {alias}")

    manifest_path = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"
    require(manifest_path.exists(), "缺少内置 skin manifest")
    manifest = manifest_path.read_text(encoding="utf-8")
    for token in ['"idle"', '"thinking"', '"speaking"', '"fallbackAction"']:
        require(token in manifest, f"manifest 缺少 {token}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
