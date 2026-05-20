#!/usr/bin/env python3
"""检查 Phase 0.71 的检察官徽章 Custom Interaction。"""

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
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    registry_cpp = read("apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    interaction_h = read("apps/desktop/src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.h")
    interaction_cpp = read("apps/desktop/src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.cpp")
    registration_cpp = read("apps/desktop/src/skins/miles-edgeworth/MilesEdgeworthInteractions.cpp")
    bridge_h = read("apps/desktop/src/pet/events/PetEventBridge.h")
    bridge_cpp = read("apps/desktop/src/pet/events/PetEventBridge.cpp")
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")
    handfeel_record = read("docs/v2/阶段记录/v1 手感回归与定制化接入.md")
    phase_record = read("docs/v2/阶段记录/第0阶段桌面壳验证.md")

    custom_interactions = manifest.get("customInteractions", [])
    custom_ids = {
        item.get("id") if isinstance(item, dict) else item
        for item in custom_interactions
    }
    require("prosecutor_badge" in custom_ids, "manifest.customInteractions 应声明 prosecutor_badge")

    config = manifest.get("customInteractionConfig", {}).get("prosecutor_badge", {})
    for key in [
        "takeThatProbability",
        "takeThatRecipe",
        "badgePropId",
        "clickedRecipe",
        "expiredRecipe",
    ]:
        require(key in config, f"prosecutor_badge 配置缺少 {key}")
    require(0 < config.get("takeThatProbability") < 1, "takeThatProbability 应是 0-1 之间的概率")
    require(config.get("takeThatRecipe") == "doubleClick.takeThat", "takeThatRecipe 应复用 manifest recipe")
    require(config.get("badgePropId") == "prosecutor_badge", "badgePropId 应指向 manifest props.prosecutor_badge")

    double_click = manifest.get("clickBehaviors", {}).get("doubleClick", [])
    require(double_click and double_click[0].get("customInteraction") == "prosecutor_badge", "双击入口应先经过 prosecutor_badge CI")
    require(any(entry.get("pool") == "doubleClick.random" for entry in double_click[1:]), "CI 不接管时应回落到 doubleClick.random")

    default_pool_recipes = {
        entry.get("recipe")
        for entry in manifest.get("actionPools", {}).get("doubleClick.random", {}).get("entries", [])
    }
    require("doubleClick.takeThat" not in default_pool_recipes, "默认双击池不应再随机触发丢徽章")
    require("doubleClick.takeThat" in manifest.get("recipes", {}), "takeThat recipe 应保留，供直接播放和兼容测试使用")

    for token in [
        "class ProsecutorBadgeInteraction",
        "CustomInteraction",
        "PetEventType::PointerDoubleClick",
        "PetEventType::PropClicked",
        "PetEventType::PropExpired",
        "host.manifestConfig()",
        "host.random()",
        "host.emitRecipe(takeThatRecipe)",
        "host.hideCurrentProp",
        "host.emitRecipe",
        "host.skipDefault",
    ]:
        require(token in interaction_h + interaction_cpp, f"ProsecutorBadgeInteraction 缺少实现点：{token}")

    for forbidden in [
        "PetRuntime *",
        "PetRuntime*",
        "PropController",
    ]:
        require(forbidden not in interaction_h + interaction_cpp, f"CI 不应直接依赖运行时私有对象：{forbidden}")

    for forbidden in [
        "prosecutor_badge",
        "doubleClick.takeThat",
        "takeThatProbability",
    ]:
        require(forbidden not in registry_cpp + pipeline_cpp + runtime_cpp, f"通用层不应写死 Miles 徽章逻辑：{forbidden}")

    require("registerMilesEdgeworthInteractions" in registration_cpp, "Miles 皮肤应有独立交互注册入口")
    require("ProsecutorBadgeInteraction" in registration_cpp, "Miles 注册入口应注册 ProsecutorBadgeInteraction")
    require("registerMilesEdgeworthInteractions(petRuntime.manifest())" in read("apps/desktop/src/main.cpp"), "main.cpp 应注册 Miles 示例皮肤交互")

    for token in [
        "submitDoubleClickForTest",
        "PetEvent::pointerDoubleClick(randomValue)",
    ]:
        require(token in bridge_h + bridge_cpp, f"PetEventBridge 应支持确定性双击随机测试：{token}")

    for token in [
        "registerMilesEdgeworthInteractions(runtime.manifest())",
        "bridge.submitDoubleClickForTest(0.0)",
        "bridge.submitDoubleClickForTest(0.99)",
        "runtime.currentPropId() == \"prosecutor_badge\"",
        "徽章 CI 应沿用当前语音语言选择看招音频",
        "点击徽章后应触发鞠躬",
        "徽章自然消失后应触发捡徽章",
    ]:
        require(token in smoke_test, f"Smoke 测试缺少 Phase 0.71 覆盖：{token}")

    for token in [
        "src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.cpp",
        "src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.h",
        "src/skins/miles-edgeworth/MilesEdgeworthInteractions.cpp",
        "src/skins/miles-edgeworth/MilesEdgeworthInteractions.h",
    ]:
        require(token in desktop_cmake, f"桌面 CMake 缺少 Miles CI 文件：{token}")

    require("check_phase_0_71_prosecutor_badge_interaction" in root_cmake, "CTest 未注册 Phase 0.71 检查")
    require("Phase 0.71" in phase_record, "阶段记录应登记 Phase 0.71")
    require("已回归（Custom Interaction）" in handfeel_record, "v1 手感记录应标记双击看招已通过 CI 回归")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
