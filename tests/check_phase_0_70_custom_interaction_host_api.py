#!/usr/bin/env python3
"""检查 Phase 0.70 的 CustomInteraction Host API 主干。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    registry_h = read("apps/desktop/src/pet/interaction/CustomInteractionRegistry.h")
    registry_cpp = read("apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    action_request_h = read("apps/desktop/src/pet/requests/ActionRequest.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    prop_controller_h = read("apps/desktop/src/pet/effects/PropController.h")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    main_cpp = read("apps/desktop/src/main.cpp")
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    guardrail = read("tests/check_phase_0_65_architecture_guardrails.cmake")
    root_cmake = read("CMakeLists.txt")
    playback_design = read("docs/v2/设计方案/皮肤包播放行为设计.md")
    split_design = read("docs/v2/设计方案/桌宠运行时职责拆分设计.md")
    phase_record = read("docs/v2/阶段记录/第0阶段桌面壳验证.md")
    handfeel_record = read("docs/v2/阶段记录/v1 手感回归与定制化接入.md")

    for token in [
        "class CustomInteraction",
        "class CustomInteractionHostApi",
        "struct CustomInteractionOutcome",
        "class CustomInteractionRegistry",
        "supportedEvents() const",
        "handleEvent(",
        "registerInteraction",
        "registerBuiltins",
        "clearForTest",
        "stopPropagation",
    ]:
        require(token in registry_h, f"CustomInteractionRegistry.h 缺少接口：{token}")

    for token in [
        "emitAction",
        "emitRecipe",
        "emitPool",
        "emitReturnToIdle",
        "spawnProp",
        "playSound",
        "random()",
        "scheduleAfter",
        "snapshot() const",
        "manifestConfig",
        "setState",
        "getState",
        "hasState",
        "skipDefault",
        "stopPropagation",
    ]:
        require(token in registry_h + registry_cpp, f"Host API 缺少能力：{token}")

    for forbidden in [
        "PetRuntime *",
        "PetRuntime*",
        "miles.feedTea",
        "prosecutor_badge",
        "doubleClick.takeThat",
        "Q_UNUSED(event)",
    ]:
        require(forbidden not in registry_h + registry_cpp, f"CustomInteractionRegistry 不应包含：{forbidden}")

    for token in [
        "std::unique_ptr<CustomInteraction>",
        "QTimer::singleShot",
        "QRandomGenerator",
        "try",
        "catch",
        "supportedEvents().contains",
        "manifest.customInteractions",
        "manifest.customInteractionConfigs",
    ]:
        require(token in registry_h + registry_cpp, f"Registry 实现缺少：{token}")

    for token in [
        "ActionRequestKind::SpawnProp",
        "ActionRequestKind::PlaySound",
        "static ActionRequest spawnProp",
        "static ActionRequest playSound",
        "QVariantMap options",
    ]:
        require(token in action_request_h, f"ActionRequest 缺少 Host API 请求类型：{token}")

    for token in [
        "ActionRequestKind::SpawnProp",
        "ActionRequestKind::PlaySound",
        "spawnFromRequest",
        "request.options",
    ]:
        require(token in runtime_cpp + prop_controller_h, f"Runtime/PropController 缺少 Host API 请求执行：{token}")

    for token in [
        "customInteractions",
        "customInteractionConfigs",
    ]:
        require(token in manifest_h + loader_cpp, f"SkinManifest 未解析 CustomInteraction 配置：{token}")

    require("CustomInteractionRegistry::registerBuiltins(petRuntime.manifest())" in main_cpp, "main.cpp 应按 manifest 注册内置 CI")
    require("CustomInteractionRegistry::handleEvent" in pipeline_cpp, "InteractionPipeline 应先分发 CustomInteraction")

    for token in [
        "ObserverCustomInteraction",
        "SkipDefaultCustomInteraction",
        "CustomInteractionRegistry::registerInteraction",
        "CustomInteractionRegistry::clearForTest",
        "bridge.submitDoubleClick()",
        "runtime.currentActionId() == \"bow\"",
    ]:
        require(token in smoke_test, f"Smoke 测试缺少 CI 行为覆盖：{token}")

    require("Q_UNUSED(event)" not in guardrail, "Phase 0.70 已收口后，0.65 门禁不应继续记录 Registry 空壳债务")
    require("WILL_FAIL TRUE" not in root_cmake, "Phase 0.65 门禁已无 expected-fail 项，应取消 WILL_FAIL")
    require("check_phase_0_70_custom_interaction_host_api" in root_cmake, "CTest 未注册 Phase 0.70 检查")
    require("Phase 0.70" in split_design and "Phase 0.70" in phase_record, "设计文档和阶段记录应登记 Phase 0.70")
    require("整个 Registry 是 14 行空壳" not in handfeel_record, "v1 手感记录不应继续保留已收口的 Registry 空壳条目")
    require("Host API 第一版" in playback_design and "CustomInteractionHostApi" in playback_design, "播放行为设计应保留 Host API 长期 spec")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
