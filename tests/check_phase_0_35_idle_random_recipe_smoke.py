#!/usr/bin/env python3
"""检查随机 idle 非移动候选已进入 PetRuntime smoke 验证。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

IDLE_RECIPES = [
    "idle.randomThinking",
    "turn.once",
    "idle.tappingHead",
    "idle.shrug",
    "idle.checkWatch",
    "idle.pointing",
    "idle.sittingTea",
    "idle.phoneCall",
    "idle.lookBack",
    "idle.lookDown",
    "idle.lookUp",
]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")

    for token in [
        "RecipeActionCase",
        "requireRecipeAction",
        "idleRecipeCases",
        "随机 idle 非移动候选应播放预期动作",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少随机 idle 候选覆盖：{token}")

    for recipe_id in IDLE_RECIPES:
        require(f'"{recipe_id}"' in smoke_test, f"PetRuntimeSmoke 缺少随机 idle 候选 {recipe_id}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_35_idle_random_recipe_smoke" in root_cmake, "CTest 未注册 Phase 0.35 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
