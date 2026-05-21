#!/usr/bin/env python3
"""Check the Phase 2.1 OpenAI-compatible provider contract."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    file_path = ROOT / path
    if not file_path.exists():
        raise AssertionError(f"missing file: {path}")
    return file_path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    config_go = read("apps/agent-core/internal/chat/config/config.go")
    config_test = read("apps/agent-core/internal/chat/config/config_test.go")
    parser_go = read("apps/agent-core/internal/chat/expression/parser.go")
    parser_test = read("apps/agent-core/internal/chat/expression/parser_test.go")
    openai_go = read("apps/agent-core/internal/chat/openai/provider.go")
    openai_test = read("apps/agent-core/internal/chat/openai/provider_test.go")
    main_go = read("apps/agent-core/cmd/miles-agent/main.go")
    server_go = read("apps/agent-core/internal/api/server.go")
    provider_go = read("apps/agent-core/internal/chat/provider.go")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    controller_smoke = read("apps/desktop/tests/chat_controller_smoke.cpp")
    root_cmake = read("CMakeLists.txt")
    index_doc = read("docs/v2/文档索引.md")
    phase_record = read("docs/v2/阶段记录/Phase 2.1 OpenAI-compatible Provider.md")

    require("MILES_PROVIDER_BASE_URL" in config_go, "config must read MILES_PROVIDER_BASE_URL")
    require("MILES_PROVIDER_API_KEY" in config_go, "config must read MILES_PROVIDER_API_KEY")
    require("MILES_PROVIDER_MODEL" in config_go, "config must read MILES_PROVIDER_MODEL")
    require("func (c ProviderConfig) Enabled()" in config_go, "config must expose Enabled receiver method")
    require("TestFromEnv" in config_test, "config tests must cover FromEnv")

    require("type Parser struct" in parser_go, "expression parser must be a struct type")
    require("Feed(chunk string)" in parser_go, "parser must expose Feed")
    require("Flush()" in parser_go, "parser must expose Flush")
    require("OnText" in parser_go and "OnExpression" in parser_go, "parser callbacks must be exposed")
    require("[EXPR:" in parser_go, "parser must reference the EXPR marker syntax")
    require("TestParserCrossChunk" in parser_test, "parser tests must cover cross-chunk markers")
    require("TestParserUnknownTag" in parser_test, "parser tests must cover unknown tags")

    require("/v1/chat/completions" in openai_go, "openai provider must POST to /v1/chat/completions")
    require("Bearer " in openai_go, "openai provider must send Bearer token")
    require("buildSystemPrompt" in openai_go, "openai provider must assemble a system prompt")
    require("expression.NewParser" in openai_go, "openai provider must use the expression parser")
    require("httptest.NewServer" in openai_test, "openai provider tests must use httptest")

    require("openai.NewProvider" in main_go, "main must construct openai provider when configured")
    require("mock-fallback" in main_go, "main must label provider as mock-fallback when config missing")

    require("http.MaxBytesReader" in server_go, "server must enforce max request body size")
    require("const DefaultListenAddr" in server_go or "DefaultListenAddr =" in server_go,
            "server must expose a shared default listen address constant")

    require('json:"conversationId"' in provider_go, "Request must use json tag conversationId")
    require('json:"message"' in provider_go, "Request must use json tag message")
    require('json:"expressions,omitempty"' in provider_go, "Request must carry expressions list")
    require("type ExpressionInfo struct" in provider_go, "ExpressionInfo struct must exist")

    require("m_holdBuffer" in controller_h, "ChatController must declare a hold buffer member")
    require("flushHoldBuffer" in controller_h, "ChatController must declare flushHoldBuffer")
    require("m_holdBuffer" in controller_cpp, "ChatController.cpp must use the hold buffer")
    require('"expressions"' in controller_cpp, "ChatController must include expressions field in request body")

    require("hold buffer" in controller_smoke.lower() or "m_holdBuffer" in controller_smoke,
            "smoke test must cover the hold buffer path")

    require("check_phase_2_1_provider" in root_cmake, "root CMake must register Phase 2.1 contract check")
    require("Phase 2.1" in phase_record, "phase 2.1 record must exist")
    require("Phase 2.1" in index_doc, "doc index must link Phase 2.1 record")

    print("phase 2.1 provider contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
