#!/usr/bin/env python3
"""检查旧版四档尺寸菜单和 scale 行为已经接入 v2。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def read_from_git(ref: str, path: str) -> str:
    import subprocess

    return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    old_header = read_from_git("main", "MilesEdgeworth.h")
    for token in [
        "const double MINI = 1",
        "const double SMALL = 1.5",
        "const double MEDIAN = 2",
        "const double BIG = 3",
    ]:
        require(token in old_header, f"旧版尺寸常量缺失：{token}")

    runtime_header = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "Q_PROPERTY(QString petSizeId READ petSizeId NOTIFY petScaleChanged)",
        "Q_PROPERTY(double petScale READ petScale NOTIFY petScaleChanged)",
        "Q_PROPERTY(double petWindowSize READ petWindowSize NOTIFY petScaleChanged)",
        "Q_PROPERTY(double petImageSize READ petImageSize NOTIFY petScaleChanged)",
        "Q_INVOKABLE void setPetSize",
        "petScaleChanged",
    ]:
        require(token in runtime_header, f"PetRuntime.h 缺少尺寸入口：{token}")

    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "m_manifest.sizes",
        "size.id == normalizedSizeId",
        "movementScaleFactor()",
        "movementDelta * movementScaleFactor()",
    ]:
        require(token in runtime_cpp, f"PetRuntime.cpp 缺少旧版尺寸逻辑：{token}")

    manifest = read("apps/desktop/resources/skins/miles-edgeworth/manifest.json")
    for token in [
        '"defaultSize": "medium"',
        '"id": "mini"',
        '"id": "small"',
        '"id": "medium"',
        '"id": "big"',
        '"label": "迷你"',
        '"label": "小"',
        '"label": "中"',
        '"label": "大"',
        '"scale": 1.0',
        '"scale": 1.5',
        '"scale": 2.0',
        '"scale": 3.0',
    ]:
        require(token in manifest, f"manifest 缺少旧版尺寸配置：{token}")

    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        'menu.addMenu(QStringLiteral("调整大小"))',
        "runtime->availablePetSizes()",
        "runtime->setPetSize(sizeId)",
        "m_runtime->petWindowSize()",
        "setFixedSize(windowSize, windowSize)",
        "m_runtime->petImageSize()",
        "m_petLabel->setGeometry(",
    ]:
        require(token in menu_cpp + surface_cpp, f"原生菜单 / 表面缺少尺寸菜单或绑定：{token}")

    smoke = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        'runtime.setPetSize("mini")',
        'runtime.setPetSize("small")',
        'runtime.setPetSize("medium")',
        'runtime.setPetSize("big")',
        "mini walk.east 应按旧版 scale=1 移动",
        "big walk.east 应按旧版 scale=3 移动",
        "默认窗口尺寸应对应旧版中号 scale=2",
    ]:
        require(token in smoke, f"PetRuntimeSmoke 缺少尺寸行为覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_42_legacy_size_menu" in root_cmake, "CTest 未注册 Phase 0.42 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
