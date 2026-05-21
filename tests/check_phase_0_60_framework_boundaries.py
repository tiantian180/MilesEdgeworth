#!/usr/bin/env python3
"""Phase 0.60: 框架边界检查。

这个检查防止 Miles 皮肤定制逻辑继续混入通用 C++ 框架层。
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
    required_files = [
        "apps/desktop/src/pet/commands/SkinCommand.h",
        "apps/desktop/src/pet/commands/SkinCommandResolver.h",
        "apps/desktop/src/pet/commands/SkinCommandResolver.cpp",
        "apps/desktop/src/pet/effects/PropController.h",
        "apps/desktop/src/pet/effects/PropController.cpp",
        "apps/desktop/src/window/WindowInputMaskController.h",
        "apps/desktop/src/window/WindowInputMaskController.cpp",
    ]
    for path in required_files:
        require((ROOT / path).is_file(), f"缺少框架边界文件：{path}")

    cpp_sources = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (ROOT / "apps/desktop/src").rglob("*")
        if path.suffix in {".cpp", ".h", ".mm"}
    )
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    manifest = read("apps/desktop/resources/skins/miles-edgeworth/manifest.json")
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    cmake = read("apps/desktop/CMakeLists.txt")

    for forbidden in [
        "miles.feedTea",
        "kFeedTeaCommandId",
        "prosecutorBadgeWindow",
        "prosecutorBadgeImage",
    ]:
        require(forbidden not in cpp_sources, f"通用 C++ 框架层不应硬编码 Miles 定制内容：{forbidden}")
        require(forbidden not in menu_cpp + surface_cpp, f"通用表现层不应硬编码 Miles 定制内容：{forbidden}")

    require('"skinCommands"' in manifest, "Miles manifest 应声明 skinCommands")
    require('"miles.feedTea"' in manifest, "Miles 红茶命令应只存在于皮肤 manifest")
    require("currentPropStartOffsetX" not in runtime_h, "Prop 展示字段不应继续挂在 PetRuntime.h")
    require("spawnPropForRecipe" not in runtime_h, "Prop 生命周期不应继续由 PetRuntime 声明")
    require("schedulePropForRecipe" not in runtime_cpp, "Prop 调度不应继续留在 PetRuntime.cpp")
    require("SkinCommandResolver" in cmake, "桌面 CMake 应链接 SkinCommandResolver")
    require("PropController" in cmake, "桌面 CMake 应链接 PropController")
    require("WindowInputMaskController" in cmake, "桌面 CMake 应链接 WindowInputMaskController")
    require(len(runtime_cpp.splitlines()) < 850, "PetRuntime.cpp 应通过本阶段拆分降到 850 行以下")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
