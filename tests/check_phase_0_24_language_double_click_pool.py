#!/usr/bin/env python3
"""检查双击语音动作保留语言覆盖池配置。

Phase 0.72 已将语言切换恢复为可选 Audio Capability。这里守住 manifest
数据、selector 语言覆盖池，以及 Runtime 不再暴露旧的 setVoiceLanguage 入口。
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
    action_pools = manifest.get("animationPools") or manifest.get("animationPools", {})

    zh_pool = action_pools.get("doubleClick.random.zh", {})
    require(zh_pool, "中文语音应有 doubleClick.random.zh 候选池")
    zh_recipes = {entry.get("recipe") for entry in zh_pool.get("entries", [])}
    require({"doubleClick.holdIt", "doubleClick.objection"} <= zh_recipes, "中文双击池应保留普通双击语音动作")
    require("doubleClick.takeThat" not in zh_recipes, "中文双击池不应直接包含看招丢徽章")
    require("doubleClick.eureka" not in zh_recipes, "中文双击池不应包含 Eureka")

    pool_selector_h = read("apps/desktop/src/pet/selection/AnimationPoolSelector.h")
    pool_selector_cpp = read("apps/desktop/src/pet/selection/AnimationPoolSelector.cpp")
    require("resolvePoolId" in pool_selector_h, "AnimationPoolSelector 应声明按上下文解析 animation pool 的入口")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    interaction_pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    require("AnimationPoolSelector::resolvePoolId" in pet_runtime_cpp, "PetRuntime 执行 ActionRequest 时应委托 AnimationPoolSelector 解析语言覆盖池")
    for token in [
        "languagePoolId",
        'normalizedPoolId + "." + languageId',
    ]:
        require(token in pool_selector_cpp, f"AnimationPoolSelector.cpp 缺少语言感知 animation pool 实现：{token}")
    require("manifest.clickBehaviors.doubleClick" in interaction_pipeline_cpp, "InteractionPipeline.cpp 应从 manifest 读取双击随机池请求")
    require('"doubleClick.random"' not in interaction_pipeline_cpp, "双击随机池不应硬编码在 InteractionPipeline.cpp")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    require("setVoiceLanguage" not in pet_runtime_h, "Runtime 不应暴露旧的 setVoiceLanguage 入口")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_24_language_double_click_pool" in root_cmake, "CTest 未注册 Phase 0.24 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
