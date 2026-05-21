#!/usr/bin/env python3
"""检查旧版随机待机的“直接反向站立”分支。"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    manifest = json.loads((ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json").read_text(encoding="utf-8"))
    recipes = manifest.get("recipes", {})
    idle_entries = manifest.get("actionPools", {}).get("idle.random", {}).get("entries", [])
    idle_recipes = {entry.get("recipe") for entry in idle_entries}

    flip_recipe = recipes.get("idle.flipStand")
    require(flip_recipe, "recipes 缺少旧版直接反向站立 idle.flipStand")
    require(flip_recipe.get("scope") == "idle", "idle.flipStand 应属于 idle scope")
    require(flip_recipe.get("action") == "idle_stand", "idle.flipStand 应继续播放 idle_stand")
    require(flip_recipe.get("facing") == "$opposite", "idle.flipStand 应使用 $opposite 切换朝向")
    require("idle.flipStand" in idle_recipes, "idle.random 应包含 idle.flipStand")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    require("QString facing;" in manifest_h, "RecipeStep 应支持 facing 字段")
    require("resolveRecipeFacing" in pet_runtime_h, "PetRuntime.h 应声明 recipe 朝向解析 helper")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    for token in [
        'recipeObject.value("facing").toString()',
        'stepObject.value("facing").toString',
    ]:
        require(token in loader_cpp, f"SkinManifestLoader.cpp 缺少 recipe facing 解析：{token}")

    for token in [
        'resolveRecipeFacing',
        'normalizedFacing == "$opposite"',
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 recipe facing 支持：{token}")

    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        'runtime.playRecipe("idle.flipStand")',
        "idle.flipStand 应从 right 直接切到 left",
        "idle.flipStand 应从 left 直接切到 right",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少 idle.flipStand 运行时覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_27_idle_flip_stand" in root_cmake, "CTest 未注册 Phase 0.27 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
