#!/usr/bin/env python3
"""检查 Phase 0.7 的 phase-aware 动画运行时骨架。

Phase 0.7 的核心不是多接几个 GIF，而是允许一个语义动作由多个 phase
组成，例如 sleep.enter -> sleep.loop -> sleep.exit。
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
    manifest_text = json.dumps(manifest, ensure_ascii=False)
    actions = manifest.get("actions", {})
    sleep = actions.get("sleep", {})
    phases = sleep.get("phases", {})

    require("sleep" in actions, "manifest 缺少 sleep action")
    require(sleep.get("initialPhase") == "enter", "sleep.initialPhase 应为 enter")
    require(sleep.get("exitPhase") == "exit", "sleep.exitPhase 应为 exit")
    require(set(phases.keys()) >= {"enter", "loop", "exit"}, "sleep 应包含 enter/loop/exit phases")
    require(phases["enter"].get("loopMode") == "once", "sleep.enter 应播放一次")
    require(phases["enter"].get("nextPhase") == "loop", "sleep.enter 播完应进入 loop")
    require(phases["loop"].get("loopMode") == "loop", "sleep.loop 应循环")
    require(phases["exit"].get("loopMode") == "onceThenIdle", "sleep.exit 播完应回 idle")
    require(actions["objecting"]["loopMode"] == "onceThenHold", "Phase 2.4 后 objecting 应是 entry-only 定帧动作")

    for phase_id in ["enter", "loop", "exit"]:
        variants = phases[phase_id].get("variants", {})
        require("right" in variants and "left" in variants, f"sleep.{phase_id} 缺少左右朝向")
        require("clip" in variants["right"], f"sleep.{phase_id}.right 缺少 clip")
        require("clip" in variants["left"], f"sleep.{phase_id}.left 缺少 clip")

    for url in [
        "file:assets/body/rest/sleep-right.gif",
        "file:assets/body/rest/sleep-left.gif",
        "file:assets/body/rest/sleeping-right.gif",
        "file:assets/body/rest/sleeping-left.gif",
        "file:assets/body/rest/wake-right.gif",
        "file:assets/body/rest/wake-left.gif",
    ]:
        require(url in manifest_text, f"manifest 缺少动画资源 {url}")
        require((skin_root / url[len("file:"):]).is_file(), f"皮肤文件缺失 {url}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "Q_PROPERTY(QString currentPhaseId",
        "QString currentPhaseId() const",
        "currentPhaseChanged",
        "PhaseDefinition",
        "playPhase",
        "void submitActionRequest(const ActionRequest &request)",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    for token in [
        "nextPhase",
        "exitPhase",
        "m_currentPhaseId",
        "playPhase(",
        'playPhase(m_currentActionId, action.exitPhase)',
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")
    require("manifest.capabilities.rest.enterRecipeId" in pipeline_cpp, "睡觉事件应读取 rest capability 的 enterRecipe")
    require("ActionRequest::recipe(manifest.capabilities.rest.enterRecipeId)" in pipeline_cpp, "睡觉事件应由 InteractionPipeline 转成 rest recipe 请求")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    for token in [
        'eventBridge->submitMenuCommand(QStringLiteral("runtime.sleep.toggle"))',
        'm_runtime->currentLoopMode() == QStringLiteral("once")',
        "m_runtime->handleAnimationFinished()",
    ]:
        require(token in surface_cpp + menu_cpp, f"原生表面 / 菜单缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_7_phase_runtime" in root_cmake, "CTest 未注册 Phase 0.7 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
