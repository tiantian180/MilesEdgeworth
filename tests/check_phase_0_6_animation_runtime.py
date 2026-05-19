#!/usr/bin/env python3
"""检查 Phase 0.6 动画运行时骨架是否具备关键接口。

这个测试仍然是轻量静态检查：它不试图替代真实 GIF 播放验收，只守住后续
动画系统继续扩展时最容易被误删的接口和 manifest 结构。
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
    qrc = read("apps/desktop/resources/pet_assets.qrc")
    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    pet_window_qml = read("apps/desktop/qml/PetWindow.qml")
    root_cmake = read("CMakeLists.txt")

    inventory_path = ROOT / "docs/v2/参考资料/动画素材盘点.md"
    require(inventory_path.exists(), "缺少动画资源盘点文档")
    inventory = inventory_path.read_text(encoding="utf-8")
    for token in [
        "body/idle/stand-right.gif",
        "body/locomotion/walk-east.gif",
        "body/locomotion/run-east.gif",
        "body/interaction/objecting-right.gif",
        "body/gestures/thinking-right.gif",
    ]:
        require(token in inventory, f"资源盘点缺少 {token}")

    require(manifest.get("schemaVersion") == 2, "manifest 应升级到 schemaVersion 2")
    require(manifest.get("facings") == ["right", "left"], "manifest 应声明 right/left 朝向")
    require(manifest.get("defaultFacing") == "right", "默认朝向应为 right")

    actions = manifest.get("actions", {})
    for action_id in ["idle_stand", "thinking", "objecting", "bow"]:
        require(action_id in actions, f"manifest 缺少 action: {action_id}")
        action = actions[action_id]
        require("loopMode" in action, f"{action_id} 缺少 loopMode")
        require("priority" in action, f"{action_id} 缺少 priority")
        require("tags" in action, f"{action_id} 缺少 tags")
        variants = action.get("variants", {})
        require("right" in variants and "left" in variants, f"{action_id} 缺少左右朝向 variant")
        require("animation" in variants["right"], f"{action_id}.right 缺少 animation")
        require("animation" in variants["left"], f"{action_id}.left 缺少 animation")

    require(actions["objecting"]["loopMode"] == "onceThenIdle", "objecting 应是一次性动作并回到 idle")
    require(actions["bow"]["loopMode"] == "onceThenIdle", "bow 应是一次性动作并回到 idle")

    for alias in ["bow-right.gif", "bow-left.gif", "tea-right.gif", "tea-left.gif"]:
        require(f'alias="{alias}"' in qrc, f"qrc 缺少 {alias}")

    for token in [
        "Q_PROPERTY(QString currentFacing",
        "Q_PROPERTY(QString currentLoopMode",
        "Q_PROPERTY(bool currentAutoReturnToIdle",
        "Q_PROPERTY(int playbackSerial",
        "Q_INVOKABLE void setFacing",
        "Q_INVOKABLE void toggleFacing",
        "Q_INVOKABLE void testObjecting",
        "Q_INVOKABLE void testBow",
        "Q_INVOKABLE void handleAnimationFinished",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")

    for token in [
        "variantForFacing",
        "m_currentFacing",
        "m_currentLoopMode",
        "m_currentAutoReturnToIdle",
        "++m_playbackSerial",
        'setState("idle")',
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")

    for token in [
        "App.PetRuntime.toggleFacing()",
        "App.PetRuntime.testObjecting()",
        "App.PetRuntime.testBow()",
        "onCurrentFrameChanged",
        "App.PetRuntime.handleAnimationFinished()",
        "onPlaybackSerialChanged",
    ]:
        require(token in pet_window_qml, f"PetWindow.qml 缺少 {token}")

    require("check_phase_0_6_animation_runtime" in root_cmake, "CTest 未注册 Phase 0.6 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
