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

    tea_recipe = recipes.get("tea.drinkThenBow", {})
    require(tea_recipe.get("scope") == "scenario", "tea.drinkThenBow 应继续作为 scenario recipe")
    require([step.get("action") for step in tea_recipe.get("steps", [])] == ["tea", "bow", "idle_stand"], "喝茶应按 tea -> bow -> idle_stand 编排")

    sleep_recipe = recipes.get("sleep.enterLoopExit", {})
    require(sleep_recipe.get("action") == "sleep", "sleep.enterLoopExit 应播放 sleep action")

    menu_tea = action_pools.get("menu.tea", {})
    require(any(entry.get("recipe") == "tea.drinkThenBow" for entry in menu_tea.get("entries", [])), "menu.tea 应能触发喝茶 recipe")

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
