#!/usr/bin/env python3
"""Phase 0.64: 原生桌宠表面菜单和最后一帧播放回归检查。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    root_cmake = read("CMakeLists.txt")

    require(
        'setStyleSheet(QStringLiteral("background: transparent;"))' not in surface_cpp,
        "PetSurfaceWindow 不应设置会级联给 QMenu 的透明 stylesheet",
    )
    require(
        ".setStyleSheet(" not in menu_cpp and "QMenu {" not in menu_cpp,
        "右键菜单应使用 Qt 平台默认样式，不应写成自定义绘制菜单",
    )
    require(
        "QMenu menu(parent)" not in menu_cpp,
        "跨 Space 桌宠窗口不应作为 QMenu 的 QWidget parent，否则 macOS 原生 popup 可能绑定到旧 Space 或崩溃",
    )
    require(
        "QMenu menu;" in menu_cpp,
        "右键菜单应使用无 QWidget parent 的 QMenu，避免依赖桌宠 native window 的 Space 状态",
    )
    require(
        "showContextMenuQueued" in surface_cpp + surface_h
        and "QTimer::singleShot(0" in surface_cpp,
        "右键菜单不应在 mousePressEvent 中同步 exec，应排到当前鼠标事件之后显示",
    )

    for forbidden in [
        "语音语言",
        "setVoiceLanguage",
        "runtime.returnToIdle",
        "runtime.facing.toggle",
        "回到待机",
        "切到朝左",
        "切到朝右",
    ]:
        require(forbidden not in menu_cpp, f"通用原生菜单不应包含调试或皮肤定制入口：{forbidden}")

    for token in [
        "scheduleAnimationCompletion",
        "completeAnimationIfStillCurrent",
        "m_runtime->playbackSerial()",
        "m_movie->nextFrameDelay()",
        "m_movie->setPaused(true)",
        "QTimer::singleShot",
    ]:
        require(token in surface_cpp + surface_h, f"最后一帧完成处理缺少延迟保护：{token}")

    immediate_completion = (
        "if (m_runtime->currentAutoReturnToIdle()\n"
        "            || m_runtime->currentLoopMode() == QStringLiteral(\"once\")) {\n"
        "        m_runtime->handleAnimationFinished();"
    )
    require(
        immediate_completion not in surface_cpp,
        "frameChanged 到达最后一帧时不应同步切换动画，否则最后一帧会被跳过",
    )

    require("check_phase_0_64_surface_menu_regressions" in root_cmake, "CTest 未注册 Phase 0.64 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
