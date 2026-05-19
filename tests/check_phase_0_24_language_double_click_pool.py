#!/usr/bin/env python3
"""检查双击语音动作支持按语言覆盖候选池。"""

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
    require({"doubleClick.holdIt", "doubleClick.takeThat", "doubleClick.objection"} <= zh_recipes, "中文双击池应保留前三类旧版动作")
    require("doubleClick.eureka" not in zh_recipes, "中文双击池不应包含 Eureka")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    require("actionPoolIdForContext" in pet_runtime_h, "PetRuntime.h 应声明按上下文解析 action pool 的 helper")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "actionPoolIdForContext",
        "languagePoolId",
        'normalizedPoolId + "." + m_voiceLanguage',
        'playActionFromPool("doubleClick.random")',
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少语言感知 action pool 实现：{token}")

    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    for token in [
        "for (int i = 0; i < 80; ++i)",
        "中文双击不应进入 Eureka 分支",
        "runtime.handleDoubleClick()",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少中文双击候选池覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_24_language_double_click_pool" in root_cmake, "CTest 未注册 Phase 0.24 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
