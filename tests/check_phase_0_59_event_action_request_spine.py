#!/usr/bin/env python3
"""检查 Phase 0.59 的事件到 ActionRequest 架构主干。

这个检查的重点不是某个具体旧版动作，而是防止 QML 继续直接调用
PetRuntime 的行为入口。QML 应只提交事件；Runtime 只接收最终播放请求。
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
    required_files = [
        "apps/desktop/src/pet/events/PetEvent.h",
        "apps/desktop/src/pet/events/PetEventBridge.h",
        "apps/desktop/src/pet/events/PetEventBridge.cpp",
        "apps/desktop/src/pet/requests/ActionRequest.h",
        "apps/desktop/src/pet/runtime/RuntimeSnapshot.h",
        "apps/desktop/src/pet/interaction/InteractionPipeline.h",
        "apps/desktop/src/pet/interaction/InteractionPipeline.cpp",
        "apps/desktop/src/pet/interaction/CustomInteractionRegistry.h",
        "apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp",
        "apps/desktop/src/pet/commands/SkinCommandResolver.h",
        "apps/desktop/src/pet/commands/SkinCommandResolver.cpp",
    ]
    for path in required_files:
        require((ROOT / path).is_file(), f"缺少事件/request 架构文件：{path}")

    pet_event_h = read("apps/desktop/src/pet/events/PetEvent.h")
    action_request_h = read("apps/desktop/src/pet/requests/ActionRequest.h")
    runtime_snapshot_h = read("apps/desktop/src/pet/runtime/RuntimeSnapshot.h")
    bridge_h = read("apps/desktop/src/pet/events/PetEventBridge.h")
    pipeline_h = read("apps/desktop/src/pet/interaction/InteractionPipeline.h")
    custom_registry_h = read("apps/desktop/src/pet/interaction/CustomInteractionRegistry.h")
    custom_registry_cpp = read("apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp")
    skin_command_resolver_h = read("apps/desktop/src/pet/commands/SkinCommandResolver.h")
    skin_command_resolver_cpp = read("apps/desktop/src/pet/commands/SkinCommandResolver.cpp")
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    main_cpp = read("apps/desktop/src/main.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    skin_design = read("docs/v2/设计方案/皮肤包播放行为设计.md")
    runtime_design = read("docs/v2/设计方案/桌宠运行时与动画调度设计.md")
    split_design = read("docs/v2/设计方案/桌宠运行时职责拆分设计.md")
    phase_record = read("docs/v2/阶段记录/第0阶段桌面壳验证.md")

    for token in [
        "struct PetEvent",
        "enum class PetEventType",
        "PointerSingleClick",
        "PointerDoubleClick",
        "MenuCommand",
        "IdleLoopFinished",
        "PropClicked",
        "PropExpired",
    ]:
        require(token in pet_event_h, f"PetEvent.h 缺少 {token}")

    for token in [
        "struct ActionRequest",
        "enum class ActionRequestKind",
        "ActionPool",
        "Recipe",
        "Action",
        "ReturnToIdle",
        "ToggleFacing",
        "enum class InterruptHint",
        "Immediate",
        "AfterCurrent",
    ]:
        require(token in action_request_h, f"ActionRequest.h 缺少 {token}")

    for token in [
        "struct RuntimeSnapshot",
        "currentState",
        "currentActionId",
        "currentRecipeId",
        "currentPhaseId",
        "currentFacing",
        "currentPropClickedRecipeId",
        "currentPropExpiredRecipeId",
        "pointerInteractionEnabled",
    ]:
        require(token in runtime_snapshot_h, f"RuntimeSnapshot.h 缺少 {token}")

    for token in [
        "class PetEventBridge",
        "QML_NAMED_ELEMENT(PetEventBridge)",
        "QML_SINGLETON",
        "submitPrimaryClick",
        "submitDoubleClick",
        "submitMenuCommand",
        "submitIdleLoopFinished",
        "submitPropClicked",
        "submitPropExpired",
    ]:
        require(token in bridge_h, f"PetEventBridge.h 缺少 {token}")

    for token in [
        "class InteractionPipeline",
        "handleEvent",
        "HitZoneMatcher",
        "BehaviorTriggerEngine",
        "CustomInteractionRegistry",
    ]:
        require(token in pipeline_h, f"InteractionPipeline.h 缺少 {token}")

    for token in [
        "class CustomInteractionRegistry",
        "handleEvent",
        "continueDefault",
    ]:
        require(token in custom_registry_h, f"CustomInteractionRegistry.h 缺少 {token}")
    require("miles.feedTea" not in custom_registry_h + custom_registry_cpp, "CustomInteractionRegistry 不应写死 Miles 皮肤命令")
    for token in [
        "class SkinCommandResolver",
        "enabledCommands",
        "resolveCommand",
    ]:
        require(token in skin_command_resolver_h + skin_command_resolver_cpp, f"SkinCommandResolver 缺少 {token}")

    for token in [
        '#include "pet/requests/ActionRequest.h"',
        '#include "pet/runtime/RuntimeSnapshot.h"',
        "RuntimeSnapshot snapshot() const",
        "void submitActionRequest(const ActionRequest &request)",
    ]:
        require(token in runtime_h, f"PetRuntime.h 缺少 ActionRequest 入口：{token}")

    for token in [
        "RuntimeSnapshot PetRuntime::snapshot() const",
        "void PetRuntime::submitActionRequest(const ActionRequest &request)",
        "ActionRequestKind::ActionPool",
        "ActionRequestKind::Recipe",
        "ActionRequestKind::ReturnToIdle",
    ]:
        require(token in runtime_cpp, f"PetRuntime.cpp 缺少 ActionRequest 执行：{token}")

    for forbidden in [
        "App.PetRuntime.handlePrimaryClick",
        "App.PetRuntime.handleDoubleClick",
        "App.PetRuntime.triggerSkinCommand",
        "App.PetRuntime.toggleSleep",
        "App.PetRuntime.handlePropClicked",
        "App.PetRuntime.handlePropExpired",
        "App.PetRuntime.handleIdleLoopFinished",
        "App.PetRuntime.returnToIdle()",
        "App.PetRuntime.toggleFacing()",
    ]:
        require(forbidden not in surface_cpp + menu_cpp, f"表现层不应继续直连 Runtime 行为入口：{forbidden}")

    for token in [
        "m_eventBridge->submitPrimaryClick",
        "m_eventBridge->submitDoubleClick",
        "eventBridge->submitMenuCommand",
        "m_eventBridge->submitIdleLoopFinished",
        "m_eventBridge->submitPropClicked",
        "m_eventBridge->submitPropExpired",
    ]:
        require(token in surface_cpp + menu_cpp, f"表现层应通过 PetEventBridge 发事件：{token}")

    for forbidden in [
        "Q_INVOKABLE void handlePrimaryClick",
        "Q_INVOKABLE void handleDoubleClick",
        "Q_INVOKABLE void triggerSkinCommand",
        "Q_INVOKABLE void handlePropClicked",
        "Q_INVOKABLE void handlePropExpired",
        "Q_INVOKABLE void handleIdleLoopFinished",
        "Q_INVOKABLE void triggerIdle",
    ]:
        require(forbidden not in runtime_h, f"PetRuntime 不应继续暴露 QML 行为入口：{forbidden}")

    for token in [
        "PetEventBridge petEventBridge",
        "PetEventBridgeForeign::s_instance = &petEventBridge",
    ]:
        require(token in main_cpp, f"main.cpp 应创建 PetEventBridge：{token}")

    for token in [
        "src/pet/events/PetEventBridge.cpp",
        "src/pet/events/PetEventBridge.h",
        "src/pet/events/PetEvent.h",
        "src/pet/requests/ActionRequest.h",
        "src/pet/runtime/RuntimeSnapshot.h",
        "src/pet/interaction/InteractionPipeline.cpp",
        "src/pet/interaction/InteractionPipeline.h",
        "src/pet/interaction/CustomInteractionRegistry.cpp",
        "src/pet/interaction/CustomInteractionRegistry.h",
        "src/pet/commands/SkinCommandResolver.cpp",
        "src/pet/commands/SkinCommandResolver.h",
    ]:
        require(token in desktop_cmake, f"桌面 CMake 缺少新架构文件：{token}")

    require("check_phase_0_59_event_action_request_spine" in root_cmake, "CTest 未注册 Phase 0.59 检查")
    require("submitActionRequest" in smoke_test, "PetRuntimeSmoke 应覆盖 ActionRequest 入口")
    require("PetEventBridge" in smoke_test, "PetRuntimeSmoke 应覆盖事件桥入口")
    require('"runtime.returnToIdle"' in smoke_test, "PetRuntimeSmoke 应覆盖回到待机菜单事件")
    require('"runtime.facing.toggle"' in smoke_test, "PetRuntimeSmoke 应覆盖切换朝向菜单事件")

    combined_design = skin_design + runtime_design + split_design
    require('"interruptHint": "replace"' not in combined_design, "长期设计文档不应继续使用 replace")
    require("interruptHint = replace" not in combined_design, "长期设计文档不应继续使用 replace")
    require("immediate" in combined_design and "afterCurrent" in combined_design, "长期设计文档应统一 immediate / afterCurrent")
    require("ActionRequest" in skin_design and "PetEvent" in runtime_design, "长期设计文档应区分事件和 ActionRequest")
    require("Phase 0.59" in split_design, "职责拆分设计应记录 Phase 0.59")
    require("Phase 0.59" in phase_record, "阶段记录应登记 Phase 0.59")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
