#!/usr/bin/env python3
"""检查 Phase 0.5 的 Pet Runtime 最小纵切是否接线完整。

这里做的是轻量静态检查，不替代真实 UI 验证。它的目的主要是防止后续重构时
不小心退回到 QML 写死 GIF、或者退回到 context property 注入对象的旧写法。
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
    pet_window_qml = read("apps/desktop/qml/PetWindow.qml")
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

    require("desktopShell." not in pet_window_qml, "PetWindow.qml 应使用 DesktopShell singleton")
    require("App.DesktopShell.alwaysOnTop" in pet_window_qml, "菜单应读取 DesktopShell.alwaysOnTop")
    require("App.DesktopShell.toggleAlwaysOnTop()" in pet_window_qml, "菜单应调用 DesktopShell.toggleAlwaysOnTop()")
    require("App.PetRuntime.currentAnimationUrl" in pet_window_qml, "动画源应绑定 PetRuntime.currentAnimationUrl")
    require('App.PetEventBridge.submitMenuCommand("runtime.returnToIdle")' in pet_window_qml, "菜单应提供回到待机入口")
    require('source: "qrc:/pet/stand-right.gif"' not in pet_window_qml, "QML 不应直接写死待机 GIF")

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
