#!/usr/bin/env python3
"""检查喝茶入口已经从开发测试入口收敛为皮肤命令。"""

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
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    bridge_h = read("apps/desktop/src/pet/events/PetEventBridge.h")
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    resolver_cpp = read("apps/desktop/src/pet/commands/SkinCommandResolver.cpp")
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    pet_window_qml = read("apps/desktop/qml/PetWindow.qml")

    require("testTea" not in runtime_h + runtime_cpp + smoke_test + pet_window_qml, "测试喝茶入口应移除")
    require("requestTea" not in runtime_h + runtime_cpp + smoke_test + pet_window_qml, "喝茶不应继续暴露 requestTea")
    require("tea.drinkThenBow" not in runtime_cpp, "测试喝茶入口不应引用已废弃的 tea.drinkThenBow")
    require("submitMenuCommand" in bridge_h + smoke_test + pet_window_qml, "喝茶应通过事件桥提交菜单命令")
    require("miles.feedTea" not in runtime_h + runtime_cpp + bridge_h + pet_window_qml, "Miles 红茶命令不应写死在通用 Runtime/QML")

    skin_command = manifest.get("skinCommands", {}).get("miles.feedTea", {})
    require(skin_command.get("request", {}).get("pool") == "menu.tea", "Miles 红茶命令应在 manifest 中复用 menu.tea 候选池")
    require("SkinCommandResolver" in resolver_cpp, "红茶皮肤命令应通过 SkinCommandResolver 解析")
    require("command.request" in resolver_cpp, "SkinCommandResolver 应返回 manifest 配置的 ActionRequest")

    require("bridge.submitMenuCommand(\"miles.feedTea\");" in smoke_test, "PetRuntimeSmoke 应覆盖红茶皮肤命令")
    require("红茶皮肤命令应从两组喝茶 recipe 中选择" in smoke_test, "PetRuntimeSmoke 应验证红茶候选池")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_40_test_tea_entry" in root_cmake, "CTest 未注册 Phase 0.40 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
