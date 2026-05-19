#!/usr/bin/env python3
"""检查 Phase 0.46 使用旧版身体点位限制窗口边界。"""

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
        "QPoint topLeft = newPos + QPoint(36 * scale, 10 * scale)",
        "QPoint topRight = newPos + QPoint(63 * scale, 10 * scale)",
        "QPoint bottomLeft = newPos + QPoint(36 * scale, 90 * scale)",
        "QPoint bottomRight = newPos + QPoint(63 * scale, 90 * scale)",
        "xMax = nowScreenRect.x() + nowScreenRect.width() - 63 * scale",
        "xMin = nowScreenRect.x() - 36 * scale",
        "yMax = nowScreenRect.y() + nowScreenRect.height() - 90 * scale",
        "yMin = nowScreenRect.y() - 10 * scale",
    ]:
        require(token in old_cpp, f"旧版身体边界缺少参考 token：{token}")

    shell_h = read("apps/desktop/src/DesktopShellController.h")
    for token in [
        "void setPetScale(double petScale)",
        "double m_petScale = 2.0",
    ]:
        require(token in shell_h, f"DesktopShellController.h 缺少 scale 同步声明：{token}")

    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    for token in [
        "DesktopShellController::setPetScale",
        "QPointF legacyBodyCenter",
        "50.0 * m_petScale",
        "36.0 * m_petScale",
        "63.0 * m_petScale",
        "10.0 * m_petScale",
        "90.0 * m_petScale",
        "screenGeometry",
        "geometry()",
    ]:
        require(token in shell_cpp, f"DesktopShellController.cpp 缺少旧版身体边界实现：{token}")

    require(
        "availableGeometry.x() + availableGeometry.width() - windowSize.width()" not in shell_cpp,
        "移动边界不应再按整个透明窗口尺寸夹取",
    )

    main_cpp = read("apps/desktop/src/main.cpp")
    for token in [
        "shellController.setPetScale(petRuntime.petScale())",
        "&PetRuntime::petScaleChanged",
    ]:
        require(token in main_cpp, f"main.cpp 缺少 PetRuntime scale 同步：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_46_legacy_body_bounds" in root_cmake, "CTest 未注册 Phase 0.46 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
