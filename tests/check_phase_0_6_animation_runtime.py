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


def require_action_variants(action: dict, action_id: str) -> None:
    variants = action.get("variants", {})
    require("right" in variants and "left" in variants, f"{action_id} 缺少左右朝向 variant")
    require("clip" in variants["right"], f"{action_id}.right 缺少 clip")
    require("clip" in variants["left"], f"{action_id}.left 缺少 clip")


def main() -> int:
    manifest_path = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    skin_root = manifest_path.parent
    manifest_text = json.dumps(manifest, ensure_ascii=False)
    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
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

    require(manifest.get("schemaVersion", 0) >= 2, "manifest schemaVersion 不应低于 2")
    require(manifest.get("facings") == ["right", "left"], "manifest 应声明 right/left 朝向")
    require(manifest.get("defaultFacing") == "right", "默认朝向应为 right")

    actions = manifest.get("actions", {})
    for action_id in ["idle_stand", "thinking", "objecting", "bow"]:
        require(action_id in actions, f"manifest 缺少 action: {action_id}")
        action = actions[action_id]
        require("label" in action, f"{action_id} 缺少 label")
        if action_id == "thinking":
            phases = action.get("phases", {})
            require(set(phases.keys()) >= {"enter", "loop", "exit"}, "thinking 应声明 enter/loop/exit phases")
            for phase_id in ["enter", "loop", "exit"]:
                phase = phases[phase_id]
                require("loopMode" in phase, f"thinking.{phase_id} 缺少 loopMode")
                require_action_variants(phase, f"thinking.{phase_id}")
        else:
            require("loopMode" in action, f"{action_id} 缺少 loopMode")
            require_action_variants(action, action_id)

    require(actions["objecting"]["loopMode"] == "onceThenHold", "objecting 应播放一次后定帧")
    require(actions["bow"]["loopMode"] == "onceThenHold", "bow 应播放一次后定帧")

    for url in [
        "file:assets/body/interaction/bow-right.gif",
        "file:assets/body/interaction/bow-left.gif",
        "file:assets/body/menu/tea-right.gif",
        "file:assets/body/menu/tea-left.gif",
    ]:
        require(url in manifest_text, f"manifest 缺少动画资源 {url}")
        require((skin_root / url[len("file:"):]).is_file(), f"皮肤文件缺失 {url}")

    for token in [
        "Q_PROPERTY(QString currentFacing",
        "Q_PROPERTY(QString currentLoopMode",
        "Q_PROPERTY(bool currentAutoReturnToIdle",
        "Q_PROPERTY(int playbackSerial",
        "Q_INVOKABLE void setFacing",
        "Q_INVOKABLE void toggleFacing",
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
        "handleMovieFrameChanged",
        "m_runtime->handleAnimationFinished()",
        "m_runtime->playbackSerial()",
        "restartMovieFromRuntime",
    ]:
        require(token in surface_cpp + surface_h, f"PetSurfaceWindow 缺少 {token}")

    require("check_phase_0_6_animation_runtime" in root_cmake, "CTest 未注册 Phase 0.6 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
