#!/usr/bin/env python3
"""检查双击语音动作保留语言覆盖池配置。

Phase 0.69 移除了 Runtime 里的临时 voiceLanguage 状态。语言菜单和动态语言
切换会在 Audio Capability 阶段回归；这里先守住 manifest 数据和 selector 能力。
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
    action_pools = manifest.get("actionPools", {})

    zh_pool = action_pools.get("doubleClick.random.zh", {})
    require(zh_pool, "中文语音应有 doubleClick.random.zh 候选池")
    zh_recipes = {entry.get("recipe") for entry in zh_pool.get("entries", [])}
    require({"doubleClick.holdIt", "doubleClick.objection"} <= zh_recipes, "中文双击池应保留普通双击语音动作")
    require("doubleClick.takeThat" not in zh_recipes, "中文双击池不应直接包含看招丢徽章")
    require("doubleClick.eureka" not in zh_recipes, "中文双击池不应包含 Eureka")

    pool_selector_h = read("apps/desktop/src/pet/selection/ActionPoolSelector.h")
    pool_selector_cpp = read("apps/desktop/src/pet/selection/ActionPoolSelector.cpp")
    require("resolvePoolId" in pool_selector_h, "ActionPoolSelector 应声明按上下文解析 action pool 的入口")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    interaction_pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    require("ActionPoolSelector::resolvePoolId" in pet_runtime_cpp, "PetRuntime 执行 ActionRequest 时应委托 ActionPoolSelector 解析语言覆盖池")
    for token in [
        "languagePoolId",
        'normalizedPoolId + "." + languageId',
    ]:
        require(token in pool_selector_cpp, f"ActionPoolSelector.cpp 缺少语言感知 action pool 实现：{token}")
    require("manifest.clickBehaviors.doubleClick" in interaction_pipeline_cpp, "InteractionPipeline.cpp 应从 manifest 读取双击随机池请求")
    require('"doubleClick.random"' not in interaction_pipeline_cpp, "双击随机池不应硬编码在 InteractionPipeline.cpp")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    require("setVoiceLanguage" not in pet_runtime_h, "语言切换入口应等 Audio Capability 阶段回归")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_24_language_double_click_pool" in root_cmake, "CTest 未注册 Phase 0.24 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
