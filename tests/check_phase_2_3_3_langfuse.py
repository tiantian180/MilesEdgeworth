#!/usr/bin/env python3
"""Check Phase 2.3.3 Langfuse observability contracts."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    file_path = ROOT / path
    if not file_path.exists():
        raise AssertionError(f"missing file: {path}")
    return file_path.read_text(encoding="utf-8")


def read_go_package(path: str) -> str:
    dir_path = ROOT / path
    if not dir_path.is_dir():
        raise AssertionError(f"missing directory: {path}")
    go_files = sorted(dir_path.glob("*.go"))
    if not go_files:
        raise AssertionError(f"missing Go files in directory: {path}")
    return "\n".join(file_path.read_text(encoding="utf-8") for file_path in go_files)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    provider_config_h = read("apps/desktop/src/settings/ProviderConfigFile.h")
    provider_config_cpp = read("apps/desktop/src/settings/ProviderConfigFile.cpp")
    settings_h = read("apps/desktop/src/settings/SettingsService.h")
    controller_h = read("apps/desktop/src/settings/SettingsController.h")
    controller_cpp = read("apps/desktop/src/settings/SettingsController.cpp")
    settings_qml = read("apps/desktop/qml/SettingsWindow.qml")
    chat_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    settings_smoke = read("apps/desktop/tests/settings_service_smoke.cpp")

    chat_provider_go = read("apps/agent-core/internal/chat/provider.go")
    config_go = read("apps/agent-core/internal/chat/config/config.go")
    service_go = read("apps/agent-core/internal/chat/service/service.go")
    main_go = read("apps/agent-core/cmd/miles-agent/main.go")
    observability_go = read_go_package("apps/agent-core/internal/chat/observability")
    observability_test = read("apps/agent-core/internal/chat/observability/provider_test.go")
    go_mod = read("apps/agent-core/go.mod")
    agent_cmake = read("apps/agent-core/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")

    rough_plan = read("docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md")
    stage = read("docs/v2/阶段记录/Phase 2.3.3 Langfuse 可观测性.md")
    index = read("docs/v2/文档索引.md")
    model_config_design = read("docs/v2/设计方案/模型配置与密钥存储设计.md")
    session_design = read("docs/v2/设计方案/会话历史与人设设计.md")

    require("struct LangfuseConfig" in provider_config_h, "ProviderConfigFile must expose LangfuseConfig")
    for token in ["enabled", "host", "publicKey", "secretKey", "captureContent", "extraFields"]:
        require(token in provider_config_h + provider_config_cpp, f"LangfuseConfig missing {token}")
    require("langfuseConfigFromJson" in provider_config_cpp, "settings.json must read langfuse object")
    require("langfuseConfigToJson" in provider_config_cpp, "settings.json must write langfuse object")
    require("langfuseUnknown" in settings_smoke, "settings smoke must preserve unknown langfuse fields")
    require("langfuseConfig" in settings_h, "SettingsService must expose langfuseConfig")

    for token in [
        "langfuseEnabled",
        "langfuseHost",
        "langfusePublicKey",
        "langfuseSecretKey",
        "langfuseCaptureContent",
    ]:
        require(token in controller_h + controller_cpp + settings_qml, f"SettingsController/QML missing {token}")
    require("其他设置" in settings_qml, "SettingsWindow must add the other settings tab")
    require("文字节奏" in settings_qml and "Langfuse Host" in settings_qml,
            "Other settings tab must hold text pacing and Langfuse settings")
    require(settings_qml.index("其他设置") < settings_qml.index("Langfuse Host"),
            "Langfuse settings should live under the other settings tab")

    for token in [
        "MILES_LANGFUSE_ENABLED",
        "LANGFUSE_HOST",
        "LANGFUSE_PUBLIC_KEY",
        "LANGFUSE_SECRET_KEY",
        "MILES_LANGFUSE_CAPTURE_CONTENT",
        "MILES_LANGFUSE_CAPTURE_SSE",
    ]:
        require(token in chat_cpp + config_go, f"sidecar env wiring missing {token}")
    require("LANGFUSE_SECRET_KEY" not in chat_cpp.split("launch sidecar", 1)[-1],
            "ChatController launch logging must not print Langfuse secret key")

    require("type LangfuseConfig struct" in config_go, "Go config must expose LangfuseConfig")
    require("func (c LangfuseConfig) Enabled()" in config_go, "Go config must skip incomplete Langfuse config")
    require("ConversationID" in chat_provider_go and "Operation" in chat_provider_go,
            "ChatParams must carry session and operation metadata")
    require('Operation:          "chat"' in service_go and 'Operation:      "summary"' in service_go,
            "ChatService must label stream and summary provider calls")

    require("go.opentelemetry.io/otel" in go_mod, "agent-core must depend on OpenTelemetry")
    require("go 1.22.0" in go_mod, "OpenTelemetry integration must keep the Go 1.22 baseline")
    require("GLOB_RECURSE MILES_AGENT_GO_SOURCES" in agent_cmake,
            "agent CMake must rebuild when new observability Go files are added")
    require("NewLangfuseTracerProvider" in observability_go, "observability must configure Langfuse OTLP")
    require("/api/public/otel/v1/traces" in observability_go, "Langfuse OTLP traces endpoint must be used")
    require("WrapProvider" in observability_go, "observability must wrap chat providers")
    require("langfuse.observation.type" in observability_go, "spans must be mapped as Langfuse observations")
    require("langfuse.session.id" in observability_go, "traces must carry Langfuse session id")
    require("langfuse.observation.usage_details" in observability_go, "traces must include usage details")
    require("langfuse.observation.model.name" in observability_go, "traces must include model name")
    require("system_prompt_hash" in observability_go, "traces must include prompt version metadata")
    require("CaptureContent" in observability_go and "input_chars" in observability_go,
            "wrapper must support metadata-only tracing when content capture is disabled")
    require("CaptureSSE" in observability_go and "ProviderStreamOutput" in chat_provider_go + observability_go,
            "wrapper must keep raw SSE capture behind an explicit opt-in")
    require("observability.WrapProvider" in main_go, "main must wrap configured provider")
    require("cfg.Langfuse.Enabled()" in main_go, "main must skip Langfuse when incomplete")
    langfuse_log = main_go.split('logger.Info("langfuse tracing enabled"', 1)[-1].split(")", 1)[0]
    require("SecretKey" not in langfuse_log and "LANGFUSE_SECRET_KEY" not in langfuse_log,
            "main logging must not include the Langfuse secret key")
    require("TestStreamChatCreatesLangfuseGenerationSpan" in observability_test,
            "observability tests must cover stream tracing")
    require("TestCompleteCreatesLangfuseGenerationSpanWithoutContentWhenDisabled" in observability_test,
            "observability tests must cover metadata-only summary tracing")

    require("Phase 2.3.3" in rough_plan and "已完成" in rough_plan,
            "rough plan must mark Phase 2.3.3 complete")
    require("Phase 2.3.3" in stage and "Langfuse" in stage, "Phase 2.3.3 stage record must exist")
    require("Phase 2.3.3 Langfuse 可观测性" in index, "doc index must link Phase 2.3.3")
    require("langfuse" in model_config_design and "captureContent" in model_config_design,
            "model config design must document langfuse settings.json fields")
    require("摘要请求当前走 Langfuse" in session_design,
            "session design must be updated now that Complete is traced")

    print("phase 2.3.3 langfuse observability contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
