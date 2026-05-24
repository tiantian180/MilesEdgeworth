#!/usr/bin/env python3
"""检查 Phase 0.73 的 ExpressionMapping schema 和本地请求链路。"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    file_path = ROOT / path
    if not file_path.exists():
        raise AssertionError(f"缺少文件：{path}")
    return file_path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    resolver_h = read("apps/desktop/src/pet/selection/ExpressionMappingResolver.h")
    resolver_cpp = read("apps/desktop/src/pet/selection/ExpressionMappingResolver.cpp")
    event_h = read("apps/desktop/src/pet/events/PetEvent.h")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")
    runtime_design = read("docs/v2/设计方案/桌宠运行时与动画调度设计.md")
    split_design = read("docs/v2/设计方案/桌宠运行时职责拆分设计.md")
    phase_record = read("docs/v2/阶段记录/第0阶段桌面壳验证.md")
    v1_record = read("docs/v2/阶段记录/v1 手感回归与定制化接入.md")

    require(manifest.get("schemaVersion", 0) >= 3, "manifest.schemaVersion 不应低于 3")

    expressions = {item.get("id"): item for item in manifest.get("expressions", []) if isinstance(item, dict)}
    for expression_id in ["neutral", "objection", "polite"]:
        require(expression_id in expressions, f"Miles manifest 缺少 expression：{expression_id}")
    require(expressions["objection"].get("allowedStates") == ["speaking"], "objection 应只允许 speaking 状态")
    require(expressions["neutral"].get("label") == "默认", "neutral 应有中文标签")

    mappings = manifest.get("expressionMappings", {})
    for expression_id in ["neutral", "objection", "polite"]:
        require(expression_id in mappings, f"Miles manifest 缺少 expression mapping：{expression_id}")
    require(mappings["neutral"].get("selection") == "first_available", "neutral 应使用 first_available")
    require(mappings["objection"].get("selection") == "weighted_random", "objection 应使用 weighted_random")
    require(mappings["objection"].get("fallback") == "neutral", "objection 应声明 neutral fallback")

    for token in [
        "ExpressionDefinition",
        "ExpressionMappingEntry",
        "ExpressionMappingDefinition",
        "QHash<QString, ExpressionDefinition> expressions",
        "QHash<QString, ExpressionMappingDefinition> expressionMappings",
    ]:
        require(token in manifest_h, f"SkinManifest 缺少 expression 结构：{token}")

    for token in [
        'root.value("expressions")',
        'root.value("expressionMappings")',
        "ExpressionDefinition",
        "ExpressionMappingDefinition",
        "allowedStates",
        "selection",
        "fallback",
    ]:
        require(token in loader_cpp, f"SkinManifestLoader 未解析 expression schema：{token}")

    for token in [
        "class ExpressionMappingResolver",
        "ExpressionMappingContext",
        "static ActionRequest resolve",
        "first_available",
        "weighted_random",
        "fallbackExpressionId",
        "allowedStates",
        "requestExists",
    ]:
        require(token in resolver_h + resolver_cpp, f"ExpressionMappingResolver 缺少能力：{token}")

    for token in [
        "AgentExpressionRequested",
        "QString state",
        "QString expression",
        "agentExpressionRequested",
    ]:
        require(token in event_h, f"PetEvent 缺少 agent expression 事件：{token}")

    require("PetEventType::AgentExpressionRequested" in pipeline_cpp, "InteractionPipeline 应处理 agent expression 事件")
    require("ExpressionMappingResolver::resolve" in pipeline_cpp, "InteractionPipeline 应委托 ExpressionMappingResolver")

    for token in [
        "Q_INVOKABLE void requestExpression",
        "submitExpressionRequest",
        "PetEvent::agentExpressionRequested",
        "QRandomGenerator::global()->generateDouble()",
    ]:
        require(token in runtime_h + runtime_cpp, f"PetRuntime 缺少 expression 请求入口：{token}")

    for token in [
        'runtime.submitExpressionRequest("speaking", "objection", 0.0)',
        'runtime.currentActionId() == "objecting"',
        'runtime.submitExpressionRequest("idle", "polite", 0.0)',
        'runtime.currentActionId() == "bow"',
        'runtime.submitExpressionRequest("speaking", "unknown-expression", 0.0)',
        'runtime.currentActionId() == "talking"',
        'runtime.submitExpressionRequest("unknown-state", "neutral", 0.0)',
        '未知 expression state 应回退到当前 PetState',
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少 expression 回归：{token}")

    for token in [
        "src/pet/selection/ExpressionMappingResolver.cpp",
        "src/pet/selection/ExpressionMappingResolver.h",
    ]:
        require(token in desktop_cmake, f"桌面 CMake 缺少 ExpressionMappingResolver：{token}")

    require("check_phase_0_73_expression_mapping" in root_cmake, "CTest 未注册 Phase 0.73 检查")
    require("Phase 0.73" in phase_record, "阶段记录应登记 Phase 0.73")
    require("ExpressionMapping" in runtime_design and "requestExpression" in split_design, "长期设计文档应说明 ExpressionMapping 请求链路")
    require("Phase 0.73" in v1_record and "Phase 2" in v1_record, "v1 回归记录应保留 Phase 0.73 到 Phase 2 的承接关系")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
