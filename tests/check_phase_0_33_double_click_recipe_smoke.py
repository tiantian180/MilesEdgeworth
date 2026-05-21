#!/usr/bin/env python3
"""检查四个旧版双击语音 recipe 已进入 PetRuntime smoke 验证。

旧版双击会随机触发 Hold it、Take that、Objection、Eureka。
这个检查要求 smoke 测试直接播放四个 recipe，验证动作和默认语音资源，
让“随机池包含它们”和“运行时真的能播放它们”两件事都被覆盖。
"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")

    for token in [
        'runtime.playRecipe("doubleClick.holdIt")',
        'runtime.playRecipe("doubleClick.takeThat")',
        'runtime.playRecipe("doubleClick.objection")',
        'runtime.playRecipe("doubleClick.eureka")',
        "Hold it 应播放抱臂动作",
        "Take that 应播放默认语音",
        "Objection 应播放默认语音",
        "Eureka 应播放默认语音",
        'qrc:/skins/miles-edgeworth/assets/audio/voice/holdit0.wav',
        'qrc:/skins/miles-edgeworth/assets/audio/voice/takethat0.wav',
        'qrc:/skins/miles-edgeworth/assets/audio/voice/objection0.wav',
        'qrc:/skins/miles-edgeworth/assets/audio/voice/eureka0.wav',
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少双击 recipe 行为覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_33_double_click_recipe_smoke" in root_cmake, "CTest 未注册 Phase 0.33 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
