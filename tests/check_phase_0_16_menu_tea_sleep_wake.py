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
    manifest_path = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    skin_root = manifest_path.parent
    recipes = manifest.get("recipes", {})
    action_pools = manifest.get("animationPools", {})

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
    rest = manifest.get("capabilities", {}).get("rest", {})
    require(rest.get("enterRecipe") == "sleep.enterLoopExit", "sleep/rest 能力应声明 enterRecipe")
    require(rest.get("loopAction") == "sleep", "sleep/rest 能力应声明 loopAction")

    skin_command = manifest.get("skinCommands", {}).get("miles.feedTea", {})
    require(skin_command.get("label") == "喂食红茶", "红茶入口应由 Miles 皮肤命令提供中文标签")
    require(skin_command.get("enabledWhen", {}).get("notAction") == "sleep", "红茶皮肤命令应在 sleep 动作中禁用")
    require(skin_command.get("request", {}).get("pool") == "menu.tea", "红茶皮肤命令应映射到 menu.tea 候选池")

    menu_tea = action_pools.get("menu.tea", {})
    require(any(entry.get("recipe") == "tea.once" for entry in menu_tea.get("entries", [])), "menu.tea 应能触发喝茶 recipe")
    require(any(entry.get("recipe") == "teaAlt.once" for entry in menu_tea.get("entries", [])), "menu.tea 应能触发第二组喝茶 recipe")
    require(len(menu_tea.get("entries", [])) >= 2, "menu.tea 应保留旧版两组喝茶动作的随机候选")

    actions = manifest.get("actions", {})
    tea_alt = actions.get("tea_alt", {})
    require(tea_alt.get("loopMode") == "onceThenIdle", "tea_alt 应播放一次后回 idle")
    variants = tea_alt.get("variants", {})
    require(variants.get("right", {}).get("clip") == "file:assets/body/menu/tea-alt-right.gif", "tea_alt.right 应使用 tea2")
    require(variants.get("left", {}).get("clip") == "file:assets/body/menu/tea-alt-left.gif", "tea_alt.left 应使用 tea3")
    for facing in ["right", "left"]:
        clip = variants.get(facing, {}).get("clip", "")
        require((skin_root / clip[len("file:"):]).is_file(), f"皮肤文件缺失 {clip}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    bridge_h = read("apps/desktop/src/pet/events/PetEventBridge.h")
    for token in [
        "Q_PROPERTY(bool sleeping READ sleeping NOTIFY sleepStateChanged)",
        "Q_PROPERTY(bool sleepTransitioning READ sleepTransitioning NOTIFY sleepStateChanged)",
        "bool sleeping() const",
        "bool sleepTransitioning() const",
        "sleepStateChanged",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")
    for token in [
        "Q_PROPERTY(QVariantList enabledSkinCommands READ enabledSkinCommands NOTIFY skinCommandAvailabilityChanged)",
        "QVariantList enabledSkinCommands() const",
        "Q_INVOKABLE void submitMenuCommand",
        "skinCommandAvailabilityChanged",
    ]:
        require(token in bridge_h, f"PetEventBridge.h 缺少 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    bridge_cpp = read("apps/desktop/src/pet/events/PetEventBridge.cpp")
    resolver_cpp = read("apps/desktop/src/pet/commands/SkinCommandResolver.cpp")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    for token in [
        "m_manifest.capabilities.rest",
        'm_currentPhaseId == QStringLiteral("loop")',
        "emit sleepStateChanged()",
    ]:
        require(token in pet_runtime_cpp + pet_runtime_h, f"PetRuntime 缺少睡眠状态语义：{token}")
    for token in [
        "QVariantList PetEventBridge::enabledSkinCommands() const",
        "SkinCommandResolver::enabledCommands",
        "void PetEventBridge::submitMenuCommand",
        "PetEventBridge::skinCommandAvailabilityChanged",
    ]:
        require(token in bridge_cpp, f"PetEventBridge.cpp 缺少 {token}")
    for token in [
        "resolveCommand",
        "command.request",
    ]:
        require(token in resolver_cpp, f"SkinCommandResolver.cpp 缺少 {token}")
    require("manifest.capabilities.rest.enterRecipeId" in pipeline_cpp, "睡眠菜单事件应读取 rest capability")
    require("ActionRequest::recipe(manifest.capabilities.rest.enterRecipeId)" in pipeline_cpp, "睡眠菜单事件应转成 rest recipe 请求")
    require("SkinCommandResolver::resolveCommand" in pipeline_cpp, "皮肤菜单命令应由 SkinCommandResolver 转成 ActionRequest")

    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    for token in [
        "eventBridge->enabledSkinCommands()",
        'menu.addMenu(QStringLiteral("皮肤动作"))',
        "eventBridge->submitMenuCommand(commandId)",
        'runtime->sleeping() ? QStringLiteral("唤醒") : QStringLiteral("睡觉")',
        "runtime->restCapabilityEnabled()",
        "enabled: !App.PetRuntime.sleepTransitioning",
    ]:
        if token.startswith("enabled:"):
            require("sleepAction->setEnabled(!runtime->sleepTransitioning())" in menu_cpp, "PetContextMenu.cpp 缺少睡眠过渡禁用逻辑")
        else:
            require(token in menu_cpp, f"PetContextMenu.cpp 缺少 {token}")
    require('eventBridge->submitMenuCommand(QStringLiteral("runtime.sleep.toggle"))' in menu_cpp, "睡眠菜单应通过事件桥提交 runtime.sleep.toggle")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_16_menu_tea_sleep_wake" in root_cmake, "CTest 未注册 Phase 0.16 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
