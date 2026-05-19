#!/usr/bin/env python3
"""Phase 0.61: PropController 职责拆分检查。"""

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
        "apps/desktop/src/pet/effects/PropState.h",
        "apps/desktop/src/pet/effects/PropController.h",
        "apps/desktop/src/pet/effects/PropController.cpp",
    ]:
        require((ROOT / path).is_file(), f"缺少 PropController 文件：{path}")

    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    prop_controller_h = read("apps/desktop/src/pet/effects/PropController.h")
    prop_controller_cpp = read("apps/desktop/src/pet/effects/PropController.cpp")
    qml = read("apps/desktop/qml/PetWindow.qml")
    cmake = read("apps/desktop/CMakeLists.txt")

    for forbidden in [
        "m_currentPropStartOffset",
        "m_currentPropEndOffset",
        "m_currentPropClickedRecipeId",
        "m_currentPropExpiredRecipeId",
        "spawnPropForRecipe",
        "schedulePropForRecipe",
        "propTravelDelta",
    ]:
        require(forbidden not in runtime_h, f"PetRuntime.h 不应持有 Prop 细节：{forbidden}")
        require(forbidden not in runtime_cpp, f"PetRuntime.cpp 不应持有 Prop 细节：{forbidden}")

    for token in [
        "class PropController",
        "scheduleForRecipe",
        "spawn",
        "hide",
        "snapshot",
        "currentPropChanged",
        "currentPropPlaybackSerialChanged",
    ]:
        require(token in prop_controller_h + prop_controller_cpp, f"PropController 缺少能力：{token}")

    require("PropController m_propController" in runtime_h, "PetRuntime 应组合 PropController")
    require("PropOverlay" in qml or "currentProp" in qml, "QML 应通过通用 Prop 表示显示附件")
    require("PropController" in cmake, "CMake 应链接 PropController")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
