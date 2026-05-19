#!/usr/bin/env python3
"""检查检察官徽章已经进入 PetRuntime smoke 验证。

旧版 `Take that` 会延迟飞出检察官徽章；徽章被点击后触发鞠躬，
自然飞完后触发捡徽章。这个检查要求 smoke 测试真实等待 Prop 生成，
并覆盖点击和自然消失两条分支。
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
        "waitForMilliseconds",
        "runtime.currentPropVisible()",
        'runtime.currentPropId() == "prosecutor_badge"',
        "runtime.handlePropClicked()",
        'runtime.currentActionId() == "bow"',
        "runtime.handlePropExpired()",
        'runtime.currentActionId() == "pickup_badge"',
        "点击徽章后应触发鞠躬",
        "徽章自然消失后应触发捡徽章",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少检察官徽章行为覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_30_prosecutor_badge_smoke" in root_cmake, "CTest 未注册 Phase 0.30 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
