#!/usr/bin/env python3
"""检查 Phase 0.58 的菜单入口和定制交互边界。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    pet_window_qml = read("apps/desktop/qml/PetWindow.qml")
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    bridge_h = read("apps/desktop/src/pet/events/PetEventBridge.h")
    bridge_cpp = read("apps/desktop/src/pet/events/PetEventBridge.cpp")
    custom_cpp = read("apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp")
    resolver_cpp = read("apps/desktop/src/pet/commands/SkinCommandResolver.cpp")
    manifest = read("apps/desktop/resources/skins/miles-edgeworth/manifest.json")
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    playback_design = read("docs/v2/设计方案/皮肤包播放行为设计.md")
    runtime_split_design = read("docs/v2/设计方案/桌宠运行时职责拆分设计.md")
    phase_record = read("docs/v2/阶段记录/第0阶段桌面壳验证.md")
    root_cmake = read("CMakeLists.txt")

    require("测试" not in pet_window_qml, "右键菜单不应再暴露开发测试入口")
    require("testThinking" not in runtime_h + runtime_cpp + pet_window_qml, "testThinking 应从公开入口移除")
    require("testSpeaking" not in runtime_h + runtime_cpp + pet_window_qml, "testSpeaking 应从公开入口移除")
    require("testObjecting" not in runtime_h + runtime_cpp + pet_window_qml, "testObjecting 应从公开入口移除")
    require("testTurn" not in runtime_h + runtime_cpp + pet_window_qml, "testTurn 应从公开入口移除")
    require("testWalk" not in runtime_h + runtime_cpp + pet_window_qml, "testWalk 应从公开入口移除")
    require("testRun" not in runtime_h + runtime_cpp + pet_window_qml, "testRun 应从公开入口移除")
    require("testBow" not in runtime_h + runtime_cpp + pet_window_qml, "testBow 应从公开入口移除")
    require("testTea" not in runtime_h + runtime_cpp + pet_window_qml + smoke_test, "testTea 应从公开入口和 smoke 测试移除")
    require("testSleep" not in runtime_h + runtime_cpp + pet_window_qml, "testSleep 应从公开入口移除")
    require("testProsecutorBadge" not in runtime_h + runtime_cpp + pet_window_qml, "testProsecutorBadge 应从公开入口移除")

    for removed_token in [
        "Q_PROPERTY(bool teaEnabled",
        "bool teaEnabled() const",
        "Q_INVOKABLE void requestTea()",
        "bool PetRuntime::teaEnabled() const",
        "void PetRuntime::requestTea()",
        "App.PetRuntime.teaEnabled",
        "App.PetRuntime.requestTea()",
    ]:
        require(removed_token not in runtime_h + runtime_cpp + pet_window_qml, f"红茶不应继续使用通用 Runtime 入口：{removed_token}")

    for token in [
        "Q_PROPERTY(QVariantList enabledSkinCommands READ enabledSkinCommands NOTIFY skinCommandAvailabilityChanged)",
        "QVariantList enabledSkinCommands() const",
        "Q_INVOKABLE void submitMenuCommand",
        "void skinCommandAvailabilityChanged()",
    ]:
        require(token in bridge_h, f"PetEventBridge.h 缺少 skin command 过渡接口：{token}")

    for token in [
        "QVariantList PetEventBridge::enabledSkinCommands() const",
        "SkinCommandResolver::enabledCommands",
        "void PetEventBridge::submitMenuCommand",
    ]:
        require(token in bridge_cpp, f"PetEventBridge.cpp 缺少 skin command 实现：{token}")

    for token in [
        '"miles.feedTea"',
    ]:
        require(token in manifest, f"manifest 缺少 Miles 皮肤命令：{token}")
    for token in [
        "resolveCommand",
        "command.request",
    ]:
        require(token in resolver_cpp, f"SkinCommandResolver.cpp 缺少 skin command 解析：{token}")
    require("miles.feedTea" not in custom_cpp + bridge_cpp + pet_window_qml, "Miles 皮肤命令不应写死在通用 C++/QML")

    for token in [
        "App.PetEventBridge.enabledSkinCommands",
        "skinCommandMenu.insertItem",
        "App.PetEventBridge.submitMenuCommand(modelData.id)",
        'App.PetRuntime.sleeping ? "唤醒" : "睡觉"',
        'App.PetEventBridge.submitMenuCommand("runtime.sleep.toggle")',
    ]:
        require(token in pet_window_qml, f"QML 菜单缺少正式入口：{token}")

    for token in [
        "bridge.submitMenuCommand(\"miles.feedTea\")",
        "hasSkinCommand(bridge.enabledSkinCommands(), \"miles.feedTea\")",
        "runtime.sleep.toggle",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 应覆盖正式入口：{token}")

    require("requestTea" not in playback_design, "长期设计文档不应继续记录 requestTea 方案")
    require("teaEnabled" not in playback_design, "长期设计文档不应继续记录 teaEnabled 方案")
    require("testTea" not in playback_design, "长期设计文档不应继续记录测试喝茶入口")
    require("皮肤定制命令" in playback_design, "播放行为设计应解释皮肤定制命令")
    require("Custom Interaction" in playback_design, "播放行为设计应解释 Custom Interaction 边界")
    require("sleep/rest" in playback_design, "播放行为设计应解释睡眠是通用 sleep/rest 能力")
    require("Phase 0.58" in runtime_split_design, "运行时职责拆分设计应记录 Phase 0.58")
    require("PlaybackController" in runtime_split_design, "运行时职责拆分设计应保留后续 PlaybackController 迁移")
    require("Phase 0.58" in phase_record, "阶段记录应登记 Phase 0.58")
    require("check_phase_0_58_menu_custom_boundary" in root_cmake, "CTest 未注册 Phase 0.58 检查")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
