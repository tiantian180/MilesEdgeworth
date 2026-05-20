#!/usr/bin/env python3
"""检查 Phase 0.12 的双击动作和音效骨架。

旧版双击会触发 Hold it / Take that / Objection / Eureka，并播放对应语音。
Phase 0.71 起，Take that 丢徽章由 Custom Interaction 概率触发，不再放在默认双击随机池里。
"""

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
    actions = manifest.get("actions", {})
    recipes = manifest.get("recipes", {})
    action_pools = manifest.get("actionPools", {})

    require("crossed" in actions, "manifest actions 缺少 crossed")
    require(actions["crossed"].get("loopMode") == "onceThenIdle", "crossed 应播放一次后回 idle")

    expected_recipes = {
        "doubleClick.holdIt": ("crossed", "qrc:/audio/holdit0.wav"),
        "doubleClick.takeThat": ("objecting", "qrc:/audio/takethat0.wav"),
        "doubleClick.objection": ("objecting", "qrc:/audio/objection0.wav"),
        "doubleClick.eureka": ("objecting", "qrc:/audio/eureka0.wav"),
    }
    for recipe_id, (action_id, sound_url) in expected_recipes.items():
        recipe = recipes.get(recipe_id, {})
        require(recipe.get("scope") == "doubleClick", f"{recipe_id} 应声明为 doubleClick recipe")
        require(recipe.get("action") == action_id, f"{recipe_id} 应映射到 {action_id}")
        require(recipe.get("sound") == sound_url, f"{recipe_id} 应播放 {sound_url}")

    double_click_pool = action_pools.get("doubleClick.random", {})
    require(double_click_pool, "actionPools 缺少 doubleClick.random")
    double_click_recipe_ids = {entry.get("recipe") for entry in double_click_pool.get("entries", [])}
    require({"doubleClick.holdIt", "doubleClick.objection", "doubleClick.eureka"} <= double_click_recipe_ids, "doubleClick.random 应包含默认双击语音动作")
    require("doubleClick.takeThat" not in double_click_recipe_ids, "Take that 丢徽章应由 Custom Interaction 概率触发")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    for alias in [
        "crossed-right.gif",
        "crossed-left.gif",
        "holdit0.wav",
        "takethat0.wav",
        "objection0.wav",
        "eureka0.wav",
    ]:
        require(f'alias="{alias}"' in qrc, f"qrc 缺少 {alias}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "Q_PROPERTY(QUrl currentSoundUrl",
        "Q_PROPERTY(int soundPlaybackSerial",
        "currentSoundUrl() const",
        "soundPlaybackSerial() const",
        "currentSoundUrlChanged",
        "soundPlaybackSerialChanged",
        "soundUrl",
        "playSoundForRecipe",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    for token in [
        "playSoundForRecipe",
        "m_currentSoundUrl",
        "m_soundPlaybackSerial",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")
    require("manifest.clickBehaviors.doubleClick" in pipeline_cpp, "双击事件应由 InteractionPipeline 读取 manifest 默认请求")
    require('"doubleClick.random"' not in pipeline_cpp, "双击默认池不应硬编码在 InteractionPipeline")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
    for token in [
        "QSoundEffect",
        "m_soundEffect",
        "m_runtime->currentSoundUrl()",
        "PetRuntime::soundPlaybackSerialChanged",
        "m_singleClickTimer",
        "m_eventBridge->submitDoubleClick()",
    ]:
        require(token in surface_cpp + surface_h, f"PetSurfaceWindow 缺少 {token}")

    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    require("Multimedia" in desktop_cmake, "CMake 应接入 Qt Multimedia")
    require("Qt6::Multimedia" in desktop_cmake, "target_link_libraries 应链接 Qt6::Multimedia")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_12_double_click_sound_runtime" in root_cmake, "CTest 未注册 Phase 0.12 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
