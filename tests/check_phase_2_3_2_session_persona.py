#!/usr/bin/env python3
"""Check Phase 2.3.2 session history and persona contracts."""

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
    design = read("docs/v2/设计方案/会话历史与人设设计.md")
    provider_go = read("apps/agent-core/internal/chat/provider.go")
    openai_go = read("apps/agent-core/internal/chat/openai/provider.go")
    server_go = read("apps/agent-core/internal/api/server.go")
    main_go = read("apps/agent-core/cmd/miles-agent/main.go")
    store_go = read_go_package("apps/agent-core/internal/store")
    models_go = read_go_package("apps/agent-core/internal/models")
    service_go = read_go_package("apps/agent-core/internal/chat/service")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    persona_store = read("apps/desktop/src/pet/manifest/PersonaStore.cpp")
    settings_h = read("apps/desktop/src/settings/SettingsController.h")
    chat_h = read("apps/desktop/src/chat/ChatController.h")
    chat_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    settings_qml = read("apps/desktop/qml/SettingsWindow.qml")
    persona = read("apps/desktop/resources/skins/miles-edgeworth/persona.md")
    root_cmake = read("CMakeLists.txt")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    agent_cmake = read("apps/agent-core/CMakeLists.txt")
    go_mod = read("apps/agent-core/go.mod")
    stage = read("docs/v2/阶段记录/Phase 2.3.2 会话历史与人设.md")
    index = read("docs/v2/文档索引.md")

    require("Persona 始终跟随当前皮肤" in design, "design must keep persona tied to current skin")
    require("PersonaPrompt" in provider_go, "chat.Request must accept personaPrompt")
    require("type Message struct" in provider_go, "chat.Message must exist")
    require("type ChatParams struct" in provider_go, "chat.ChatParams must exist")
    require("StreamChat(ctx context.Context, params ChatParams)" in provider_go, "Provider must expose StreamChat")
    require("Complete(ctx context.Context, params ChatParams)" in provider_go, "Provider must expose Complete")
    require("StreamReply" not in provider_go, "old StreamReply interface must be removed")
    require("BuildSystemPrompt" not in openai_go, "OpenAI provider must not own exported system prompt construction")
    require("buildSystemPrompt" not in openai_go, "OpenAI provider must not own unexported system prompt construction")
    require("Messages:" in openai_go and "params.Messages" in openai_go, "OpenAI provider must serialize prepared messages")
    require("Complete(" in openai_go and "chatCompletionResponse" in openai_go, "OpenAI provider must implement non-stream completion")

    require("CREATE TABLE IF NOT EXISTS conversations" in store_go, "store must create conversations table")
    require("CREATE TABLE IF NOT EXISTS messages" in store_go, "store must create messages table")
    require("PRAGMA foreign_keys = ON" in store_go, "store must enable SQLite foreign keys")
    require("ON DELETE CASCADE" in store_go, "messages must cascade with conversations")
    require("crypto/rand" in store_go, "conversation ids must be UUID v4 style random ids")
    require("AppendMessage" in store_go and "updated_at" in store_go, "AppendMessage must update conversation timestamp")
    require("ReplaceSummary" in store_go and "BeginTx" in store_go, "ReplaceSummary must be transactional")
    require("is_partial" in store_go, "store must persist assistant partial flag")
    require("modernc.org/sqlite" in go_mod, "agent-core must use modernc.org/sqlite")

    require("models.dev/api.json" in models_go, "models catalog must fetch models.dev API")
    require("models-cache.json" in models_go, "models catalog must cache responses")
    require(
        ('json:"limit"' in models_go and 'json:"context"' in models_go)
        or "limit.context" in models_go,
        "models catalog must read limit.context",
    )
    require("8192" in models_go, "models catalog must use 8192 fallback context window")

    require("type Service struct" in service_go, "ChatService must exist")
    require("BuildMessages" in service_go, "ChatService must build model messages")
    require("PersonaPrompt" in service_go or "personaPrompt" in service_go, "ChatService must use request persona")
    require("当前可用表达标签" in service_go, "ChatService must generate dynamic EXPR rules")
    require("以下是较早对话的摘要" in service_go, "summary must be injected as system context")
    require(
        "EstimateTokens" in service_go and ("len([]rune" in service_go or "utf8.RuneCountInString" in service_go),
        "token estimate must use rune count",
    )
    require("miles.chat.memory.summarizing" in service_go, "ChatService/API must emit summarizing custom event")

    require('"/v1/conversations"' in server_go, "API must register conversations endpoint")
    require('"/v1/chat/messages"' in server_go, "chat endpoint must remain")
    require("AppendMessage" in server_go, "chat endpoint must persist user and assistant messages")
    require("isPartial" in server_go, "chat endpoint must expose partial messages")
    require("MILES_DATA_DIR" in main_go, "sidecar main must read MILES_DATA_DIR")
    require("store.Open" in main_go, "sidecar main must open SQLite store")

    require("personaPrompt" in manifest_h, "SkinManifest must contain personaPrompt")
    require("PersonaStore" in loader_cpp, "SkinManifestLoader must delegate persona read to PersonaStore")
    require("skin-overrides" in persona_store, "PersonaStore must write app-dir overrides under skin-overrides")
    require("persona-overrides" not in persona_store, "legacy persona-overrides path must be removed")
    require("appSkinDirectoryPath" in persona_store, "PersonaStore must decide app-dir skin writability by appSkinDirectoryPath")
    require("manifest.builtin" not in persona_store, "PersonaStore must not use builtin flag")
    require("savePersona" in settings_h or "personaPrompt" in settings_h, "SettingsController must expose persona editing")
    require("角色人格" in settings_qml, "SettingsWindow must show persona editor")
    require(
        'id: personaField' in settings_qml
        and 'color: "#26201b"' in settings_qml
        and "wrapMode: TextArea.Wrap" in settings_qml,
        "SettingsWindow persona editor must use readable dark text",
    )
    require(persona.strip(), "Miles persona.md must exist as a filesystem skin file")
    require("[EXPR:" not in persona, "persona.md must not contain EXPR marker instructions")

    require("conversations" in chat_h and "currentConversationId" in chat_h, "ChatController must expose conversations")
    require("loadConversations" in chat_h and "switchConversation" in chat_h, "ChatController must expose conversation methods")
    require("newConversation" in chat_h and "deleteConversation" in chat_h, "ChatController must expose new/delete methods")
    require("MILES_DATA_DIR" in chat_cpp, "ChatController must inject MILES_DATA_DIR to sidecar")
    require("personaPrompt" in chat_cpp, "ChatController must send current personaPrompt")
    require("conversationId" in chat_cpp and 'QStringLiteral("default")' not in chat_cpp, "ChatController must stop hardcoding default conversation id")
    require("miles.chat.memory.summarizing" in chat_cpp, "ChatController must handle summarizing event")
    require("会话" in chat_qml and "新建" in chat_qml, "ChatWindow must expose conversation list controls")
    require(
        "id: transcriptScrollTimer" in chat_qml
        and "transcriptScrollTimer.start()" in chat_qml
        and "transcriptScrollTimer.restart()" not in chat_qml,
        "ChatWindow must throttle transcript autoscroll during streamed message updates",
    )
    require(
        "minimumReadableBubbleWidth" not in chat_qml and "stableStreamingWidth" not in chat_qml,
        "ChatWindow bubbles must not force a minimum or streaming width; padding and max width are enough",
    )
    require(
        "id: messageMeasure" in chat_qml
        and "messageMeasure.contentWidth" in chat_qml
        and "maxContentWidth" in chat_qml,
        "ChatWindow bubbles must measure wrapped text content width at the maximum line width",
    )
    require(
        "id: transcriptModel" in chat_qml
        and "syncTranscriptMessages" in chat_qml
        and "model: transcriptModel" in chat_qml
        and "model: App.ChatController.messages" not in chat_qml,
        "ChatWindow must mirror messages into a local ListModel instead of resetting the ListView on every streamed chunk",
    )

    require("PersonaStore" in desktop_cmake, "desktop CMake must compile PersonaStore")
    require("go.sum" in agent_cmake, "agent CMake must rebuild when go.sum changes")
    require("check_phase_2_3_2_session_persona" in root_cmake, "root CMake must register Phase 2.3.2 contract")
    require("Phase 2.3.2 会话历史与人设" in stage, "stage record must exist")
    require("会话历史与人设" in index, "doc index must link design/stage docs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
