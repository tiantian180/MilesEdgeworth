#!/usr/bin/env python3
"""检查 idle loop 触发概率已经从 C++ 迁移到皮肤 manifest。"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace_start = source.index("{", start)
    depth = 0
    for index in range(brace_start, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace_start : index + 1]
    raise AssertionError(f"无法截取函数体：{signature}")


def main() -> int:
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    runtime_header = read("apps/desktop/src/pet/PetRuntime.h")
    manifest_header = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    trigger_engine = read("apps/desktop/src/pet/behavior/BehaviorTriggerEngine.h")

    triggers = manifest.get("behaviorTriggers", {})
    idle_trigger = triggers.get("idle.loopFinished")
    require(isinstance(idle_trigger, dict), "manifest 应声明 behaviorTriggers.idle.loopFinished")
    require(
        idle_trigger.get("when") == {
            "state": "idle",
            "action": "idle_stand",
            "requiresNoActiveRecipe": True,
        },
        "idle.loopFinished 应声明状态、动作和无 active recipe 的触发条件",
    )

    entries = idle_trigger.get("entries", [])
    require(
        {"type": "pool", "pool": "idle.random", "weight": 70} in entries,
        "idle.loopFinished 应通过 manifest 配置 70 权重进入 idle.random",
    )
    require(
        {"type": "none", "weight": 30} in entries,
        "idle.loopFinished 应通过 manifest 配置 30 权重保持站立",
    )

    idle_body = function_body(pipeline_cpp, "QList<ActionRequest> InteractionPipeline::handleEvent(")
    require(
        "behaviorTriggerIdForEvent" in pipeline_cpp and "kIdleLoopFinishedTriggerId" in pipeline_cpp,
        "idle.loopFinished 应由 InteractionPipeline 读取 manifest behavior trigger",
    )
    require("randomValue <= 0.7" not in idle_body, "idle loop 概率不应继续硬编码在 C++ 函数里")
    require("playActionFromPool(\"idle.random\")" not in idle_body, "idle loop 目标 pool 不应继续硬编码在 C++ 函数里")

    for token in [
        "BehaviorTriggerEntry",
        "BehaviorTriggerDefinition",
        "BehaviorTriggerEngine",
    ]:
        require(token in runtime_header + manifest_header + trigger_engine + pipeline_cpp, f"运行时应提供通用 behavior trigger 结构：{token}")
    require("BehaviorTriggerEngine::selectEntry" in pipeline_cpp, "InteractionPipeline 应委托 BehaviorTriggerEngine 抽取触发结果")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_52_manifest_idle_loop_trigger" in root_cmake, "CTest 未注册 Phase 0.52 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
