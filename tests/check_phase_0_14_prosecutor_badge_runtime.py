#!/usr/bin/env python3
"""检查 Phase 0.14 的检察官徽章 Prop 骨架。

旧版双击随机到 Take that 时，会播放语音和 object 动作，并在短暂延迟后
飞出检察官徽章。徽章被点击时消失并触发鞠躬；自然飞完时消失并触发捡
徽章动作。这个检查守住资源、manifest、PetRuntime 属性入口和 QML
独立 Prop 窗口，保证后续可以继续演进成正式 Prop Runtime。
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


def main() -> None:
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))

    props = manifest.get("props", {})
    badge = props.get("prosecutor_badge")
    require(badge, "manifest 缺少 props.prosecutor_badge")
    require(badge.get("asset") == "qrc:/pet/prosecutor-badge.png", "prosecutor_badge 应声明徽章图片资源")
    require(badge.get("delayMs") == 700, "prosecutor_badge 应保留旧版 700ms 延迟")
    require(badge.get("durationMs") == 1500, "prosecutor_badge 应保留旧版 1500ms 飞行时间")
    require(badge.get("clickedRecipe") == "bow.once", "点击徽章后应触发鞠躬")
    require(badge.get("expiredRecipe") == "pickup.once", "徽章自然消失后应触发捡徽章")

    for facing in ["right", "left"]:
        require(facing in badge.get("startOffsets", {}), f"徽章缺少 {facing} 起点偏移")
        require(facing in badge.get("travel", {}), f"徽章缺少 {facing} 飞行偏移")

    actions = manifest.get("actions", {})
    pickup = actions.get("pickup_badge")
    require(pickup, "manifest 缺少 pickup_badge 动作")
    require(pickup.get("loopMode") == "onceThenIdle", "pickup_badge 应播放一次后回待机")
    pickup_urls = {variant.get("animation") for variant in pickup.get("variants", {}).values()}
    require("qrc:/pet/pickup-right.gif" in pickup_urls, "pickup_badge 缺少右向动画")
    require("qrc:/pet/pickup-left.gif" in pickup_urls, "pickup_badge 缺少左向动画")

    take_that = manifest.get("recipes", {}).get("doubleClick.takeThat", {})
    require(take_that.get("prop") == "prosecutor_badge", "doubleClick.takeThat 应触发 prosecutor_badge")
    require(manifest.get("recipes", {}).get("pickup.once", {}).get("action") == "pickup_badge", "pickup.once 应播放 pickup_badge")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    for alias in ["prosecutor-badge.png", "pickup-right.gif", "pickup-left.gif"]:
        require(alias in qrc, f"qrc 缺少 {alias}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "currentPropVisible",
        "currentPropId",
        "currentPropImageUrl",
        "currentPropStartOffsetX",
        "currentPropEndOffsetX",
        "currentPropDurationMs",
        "currentPropPlaybackSerial",
        "spawnPropForRecipe",
        "PropDefinition",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    for token in [
        "schedulePropForRecipe",
        "spawnPropForRecipe",
        "m_currentPropPlaybackSerial",
        "QTimer::singleShot",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")
    for token in [
        "PetEventType::PropClicked",
        "PetEventType::PropExpired",
        "currentPropClickedRecipeId",
        "currentPropExpiredRecipeId",
    ]:
        require(token in pipeline_cpp, f"InteractionPipeline.cpp 缺少 Prop 事件处理：{token}")

    qml = read("apps/desktop/qml/PetWindow.qml")
    for token in [
        "id: prosecutorBadgeWindow",
        "App.PetRuntime.currentPropVisible",
        "App.PetRuntime.currentPropImageUrl",
        "onCurrentPropPlaybackSerialChanged",
        "badgeFlyAnimation",
        "badgeExpireTimer",
        "App.PetEventBridge.submitPropClicked()",
        "App.PetEventBridge.submitPropExpired()",
    ]:
        require(token in qml, f"PetWindow.qml 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_14_prosecutor_badge_runtime" in root_cmake, "CTest 未注册 Phase 0.14 检查")


if __name__ == "__main__":
    main()
