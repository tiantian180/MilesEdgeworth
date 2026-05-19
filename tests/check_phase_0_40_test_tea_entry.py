#!/usr/bin/env python3
"""检查开发菜单的测试喝茶入口复用正式菜单喝茶逻辑。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")

    require("void PetRuntime::testTea()" in runtime_cpp, "PetRuntime.cpp 应保留测试喝茶入口")
    require("tea.drinkThenBow" not in runtime_cpp, "测试喝茶入口不应引用已废弃的 tea.drinkThenBow")
    require("void PetRuntime::testTea()\n{\n    requestTea();\n}" in runtime_cpp, "testTea 应复用 requestTea 的菜单喝茶逻辑")

    require("runtime.testTea();" in smoke_test, "PetRuntimeSmoke 应覆盖测试喝茶入口")
    require("测试喝茶入口也应复用菜单喝茶候选池" in smoke_test, "PetRuntimeSmoke 应验证测试喝茶候选池")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_40_test_tea_entry" in root_cmake, "CTest 未注册 Phase 0.40 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
