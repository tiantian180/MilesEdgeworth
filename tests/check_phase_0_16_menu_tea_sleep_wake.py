#!/usr/bin/env python3
"""检查 Phase 0.16 的菜单喝茶、睡觉和唤醒链路。

旧版右键菜单里，“喂食红茶”和“睡觉 / 唤醒”是桌宠生命感的一部分。
这个检查守住 v2 的菜单入口、PetRuntime 状态边界和 manifest recipe。
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
    recipes = manifest.get("recipes", {})
    action_pools = manifest.get("actionPools", {})

    tea_recipe = recipes.get("tea.once", {})
    require(tea_recipe.get("scope") == "menu", "tea.once 应作为菜单 recipe")
    require(tea_recipe.get("action") == "tea", "tea.once 应直接播放 tea GIF")
    require("steps" not in tea_recipe, "tea.once 不应额外串联 bow 或 idle_stand")

    tea_alt_recipe = recipes.get("teaAlt.once", {})
    require(tea_alt_recipe.get("scope") == "menu", "teaAlt.once 应作为第二组喝茶菜单 recipe")
    require(tea_alt_recipe.get("action") == "tea_alt", "teaAlt.once 应直接播放 tea_alt GIF")
    require("steps" not in tea_alt_recipe, "teaAlt.once 不应额外串联 bow 或 idle_stand")

    sleep_recipe = recipes.get("sleep.enterLoopExit", {})
    require(sleep_recipe.get("action") == "sleep", "sleep.enterLoopExit 应播放 sleep action")

    menu_tea = action_pools.get("menu.tea", {})
    require(any(entry.get("recipe") == "tea.once" for entry in menu_tea.get("entries", [])), "menu.tea 应能触发喝茶 recipe")
    require(any(entry.get("recipe") == "teaAlt.once" for entry in menu_tea.get("entries", [])), "menu.tea 应能触发第二组喝茶 recipe")
    require(len(menu_tea.get("entries", [])) >= 2, "menu.tea 应保留旧版两组喝茶动作的随机候选")

    actions = manifest.get("actions", {})
    tea_alt = actions.get("tea_alt", {})
    require(tea_alt.get("loopMode") == "onceThenIdle", "tea_alt 应播放一次后回 idle")
    variants = tea_alt.get("variants", {})
    require(variants.get("right", {}).get("animation") == "qrc:/pet/tea-alt-right.gif", "tea_alt.right 应使用 tea2")
    require(variants.get("left", {}).get("animation") == "qrc:/pet/tea-alt-left.gif", "tea_alt.left 应使用 tea3")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    for alias in ["tea-alt-right.gif", "tea-alt-left.gif"]:
        require(f'alias="{alias}"' in qrc, f"qrc 缺少 {alias}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "Q_PROPERTY(bool sleeping READ sleeping NOTIFY sleepStateChanged)",
        "Q_PROPERTY(bool sleepTransitioning READ sleepTransitioning NOTIFY sleepStateChanged)",
        "Q_PROPERTY(bool teaEnabled READ teaEnabled NOTIFY sleepStateChanged)",
        "bool sleeping() const",
        "bool sleepTransitioning() const",
        "bool teaEnabled() const",
        "Q_INVOKABLE void requestTea()",
        "Q_INVOKABLE void toggleSleep()",
        "sleepStateChanged",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "bool PetRuntime::sleeping() const",
        "bool PetRuntime::sleepTransitioning() const",
        "bool PetRuntime::teaEnabled() const",
        "void PetRuntime::requestTea()",
        'playActionFromPool("menu.tea")',
        "void PetRuntime::toggleSleep()",
        'playRecipe("sleep.enterLoopExit")',
        "emit sleepStateChanged()",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")

    pet_window_qml = read("apps/desktop/qml/PetWindow.qml")
    for token in [
        "喂食红茶",
        "App.PetRuntime.requestTea()",
        "enabled: App.PetRuntime.teaEnabled",
        'App.PetRuntime.sleeping ? "唤醒" : "睡觉"',
        "enabled: !App.PetRuntime.sleepTransitioning",
        "App.PetRuntime.toggleSleep()",
    ]:
        require(token in pet_window_qml, f"PetWindow.qml 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_16_menu_tea_sleep_wake" in root_cmake, "CTest 未注册 Phase 0.16 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
