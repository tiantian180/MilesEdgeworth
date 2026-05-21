#!/usr/bin/env python3
"""检查所有旧版单击分区已经进入 PetRuntime smoke 验证。

旧版单击不同身体区域会触发不同反应。此前 smoke 已覆盖脸部和 polygon 边界，
这个检查要求继续覆盖头部、大臂、小臂、胸口、肚子和腿部，保证分区优先级
和 action pool 运行路径都经过真实 PetRuntime。
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
        "requireActionIn",
        "右朝向头部左侧不应有点击空洞",
        "点击头部应触发头部候选动作",
        "点击大臂应触发转身",
        "点击小臂应触发小臂候选动作",
        "点击胸口应触发抱臂思考",
        "点击腰部上半应触发鞠躬而不是胸口动作",
        "点击肚子下半应触发指点",
        "右朝向点击左侧腿部应触发后退",
        "右朝向点击右侧腿部应触发低头看",
        "左朝向点击右侧腿部应触发后退",
        "左朝向点击左侧腿部应触发低头看",
        "fallback returnToIdle 不应重启 idle_stand 动画",
        "bridge.submitPrimaryClick(50, 40, 240, 240)",
        "bridge.submitPrimaryClick(100, 40, 240, 240)",
        "bridge.submitPrimaryClick(60, 85, 240, 240)",
        "bridge.submitPrimaryClick(60, 130, 240, 240)",
        "bridge.submitPrimaryClick(120, 85, 240, 240)",
        "bridge.submitPrimaryClick(120, 104, 240, 240)",
        "bridge.submitPrimaryClick(120, 118, 240, 240)",
        "bridge.submitPrimaryClick(95, 200, 240, 240)",
        "bridge.submitPrimaryClick(145, 200, 240, 240)",
        "bridge.submitPrimaryClick(10, 10, 240, 240)",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少单击分区行为覆盖：{token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_32_single_click_zone_smoke" in root_cmake, "CTest 未注册 Phase 0.32 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
