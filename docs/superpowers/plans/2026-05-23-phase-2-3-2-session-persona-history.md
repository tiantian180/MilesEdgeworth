# Phase 2.3.2 会话历史与人设 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Phase 2.3.2 exactly as specified in `docs/v2/设计方案/会话历史与人设设计.md`: skin-bound persona loading/editing, SQLite conversation persistence, ChatService context assembly and summarization, conversation APIs, and conversation-list UI.

**Architecture:** Qt remains responsible for current skin state and sends the current `personaPrompt` plus expression list on every `POST /v1/chat/messages`; sidecar never snapshots or caches persona. Go sidecar owns SQLite and adds three focused packages: `store` for persistence, `models` for context-window metadata, and `chat/service` for prompt/history/summary assembly. Provider stays pure: it receives prepared `[]chat.Message`, serializes them to the upstream API, parses stream output, and has no store or history dependency.

**Tech Stack:** Qt 6/C++17/QML, Go 1.22, `modernc.org/sqlite`, CMake/Ninja/CTest, Go `httptest`, Python 3 static contract checks.

---

## Scope Check

This plan implements **Phase 2.3.2** only.

Included:
- `persona.md` loading from skin packages, with built-in skin override under `$MILES_DATA_DIR/persona-overrides/{skinId}.md`.
- Settings UI editor for current skin persona, with visible save errors.
- Sidecar SQLite store at `$MILES_DATA_DIR/chat.db`.
- Conversation CRUD API and message restore API.
- ChatService context assembly, dynamic EXPR prompt injection, token-budget checks, summary replacement, and fallback truncation.
- Provider interface upgrade to prepared `[]chat.Message` plus `Complete()` for summaries.
- ChatWindow conversation list, new/delete/switch behavior, and skin mismatch hint.
- Miles Edgeworth `persona.md`.

Excluded:
- Langfuse observability: Phase 2.3.3.
- Structured Outputs or tool-calling protocol changes.
- Multi-modal attachments.
- Memory system beyond conversation summaries.
- Pagination beyond `GET /v1/conversations?limit=N` defaulting to 100.
- Animation manifest splitting or Phase 2.4 animation-detail changes.

## Non-Negotiable Design Constraints

- Persona belongs to the current skin. Qt sends `personaPrompt` on every chat request. Sidecar does not cache, snapshot, hash, or restore old persona by conversation.
- `persona.md` contains character/personality instructions only. EXPR marker rules are generated dynamically by sidecar from the request's `expressions`.
- Provider does not read SQLite and does not assemble history. Handler/ChatService reads store, builds messages, then calls provider.
- Summary rows are stored as `role='summary'` but sent to the model as part of the system prompt with prefix `以下是较早对话的摘要：`.
- `AppendMessage` and `ReplaceSummary` are transactional. `AppendMessage` also updates `conversations.updated_at`.
- Current user message and the latest turn are never summarized or fallback-truncated.
- `miles.chat.memory.summarizing` must be sent after SSE headers are written and before provider `RUN_STARTED`.
- API key and provider settings never enter SQLite or logs.

## Required Reading

- `docs/v2/设计方案/会话历史与人设设计.md`
- `docs/v2/设计方案/AI 聊天动画编排设计.md` for EXPR stream semantics and ChatController state-machine behavior.
- `docs/v2/设计方案/皮肤包分发与加载机制设计.md` for current skin loading boundaries.
- Current code:
  - `apps/agent-core/internal/api/server.go`
  - `apps/agent-core/internal/chat/provider.go`
  - `apps/agent-core/internal/chat/openai/provider.go`
  - `apps/desktop/src/chat/ChatController.{h,cpp}`
  - `apps/desktop/src/pet/manifest/SkinManifestLoader.{h,cpp}`
  - `apps/desktop/qml/ChatWindow.qml`
  - `apps/desktop/qml/SettingsWindow.qml`

## File Structure

Create:
- `apps/agent-core/internal/store/store.go`
  - SQLite schema, UUID v4 generation, conversation/message CRUD, transactional append and summary replacement.
- `apps/agent-core/internal/store/store_test.go`
  - Store tests using `t.TempDir()`.
- `apps/agent-core/internal/models/catalog.go`
  - models.dev cache, context-window lookup, fallback values.
- `apps/agent-core/internal/models/catalog_test.go`
  - `httptest` coverage for cache, stale fallback, and model field mapping.
- `apps/agent-core/internal/chat/service/service.go`
  - ChatService, system prompt assembly, token estimate, summary compression, fallback truncation.
- `apps/agent-core/internal/chat/service/service_test.go`
  - Prompt/history/summary tests with fake store/provider/model catalog.
- `apps/desktop/src/pet/manifest/PersonaStore.h`
  - Qt persona read/write helper for built-in overrides and filesystem skins.
- `apps/desktop/src/pet/manifest/PersonaStore.cpp`
  - `MILES_DATA_DIR`-compatible override path handling.
- `apps/desktop/resources/skins/miles-edgeworth/persona.md`
  - Miles persona content with no EXPR instructions.
- `tests/check_phase_2_3_2_session_persona.py`
  - Static contract check.
- `docs/v2/阶段记录/Phase 2.3.2 会话历史与人设.md`
  - Stage record and acceptance notes.

Modify:
- `apps/agent-core/go.mod`
  - Add `modernc.org/sqlite`.
- `apps/agent-core/go.sum`
  - Generated by `go mod tidy`.
- `apps/agent-core/CMakeLists.txt`
  - Make sidecar rebuild depend on `go.sum`.
- `apps/agent-core/cmd/miles-agent/main.go`
  - Open data dir, store, models catalog, ChatService, and pass them to API server.
- `apps/agent-core/internal/chat/provider.go`
  - Add `PersonaPrompt`, `Message`, `ChatParams`, `StreamChat`, and `Complete`.
- `apps/agent-core/internal/chat/mock_provider.go`
  - Implement new provider interface.
- `apps/agent-core/internal/chat/openai/provider.go`
  - Remove provider-owned system prompt building; serialize `ChatParams.Messages`.
- `apps/agent-core/internal/chat/openai/provider_test.go`
  - Verify prepared messages are sent unchanged and `Complete()` works.
- `apps/agent-core/internal/api/server.go`
  - Add conversation endpoints and persistence-aware chat streaming.
- `apps/agent-core/internal/api/server_test.go`
  - Cover conversation CRUD, message restore, chat persistence, partial persistence, and summarizing SSE order.
- `apps/desktop/src/pet/manifest/SkinManifest.h`
  - Add `personaPrompt`.
- `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
  - Load persona with override priority.
- `apps/desktop/src/settings/SettingsController.{h,cpp}`
  - Add persona properties and save/reload flow using `PetRuntime`.
- `apps/desktop/src/main.cpp`
  - Pass `PetRuntime` into SettingsController.
- `apps/desktop/src/chat/ChatController.{h,cpp}`
  - Add conversation properties/methods, create/switch/delete/load APIs, send real `conversationId`, send persona, inject `MILES_DATA_DIR`.
- `apps/desktop/qml/SettingsWindow.qml`
  - Add persona editor.
- `apps/desktop/qml/ChatWindow.qml`
  - Add conversation list panel and skin mismatch hint.
- `apps/desktop/resources/pet_assets.qrc`
  - Include `persona.md` under `/skins/miles-edgeworth`.
- `apps/desktop/CMakeLists.txt`
  - Add `PersonaStore` to app and tests.
- `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
  - Cover persona load and override priority.
- `apps/desktop/tests/settings_service_smoke.cpp`
  - Extend or add settings-controller coverage for persona save error/success.
- `apps/desktop/tests/chat_controller_smoke.cpp`
  - Cover conversation properties and custom summarizing event status.
- `CMakeLists.txt`
  - Register `check_phase_2_3_2_session_persona`.
- `docs/v2/文档索引.md`
  - Add stage record link.

Not modified:
- `docs/v2/设计方案/会话历史与人设设计.md` during implementation. If code reveals a mismatch, stop and review the design before coding around it.
- `docs/v2/设计方案/AI 聊天动画编排设计.md` unless a separate design review explicitly changes animation semantics.

---

## Task 0: Preflight and Branch Discipline

**Files:**
- Read-only.

- [ ] **Step 1: Verify branch and worktree**

Run:

```bash
git status --short --branch
```

Expected: clean worktree on `docs/phase-2-3-2-session-persona` or a feature branch derived from it. The branch must contain `docs/v2/设计方案/会话历史与人设设计.md`.

- [ ] **Step 2: Verify baseline tests**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
ctest --test-dir build -R "check_phase_2_3_1_animation_sync|check_logging_standard|chat_controller_smoke|skin_manifest_loader_smoke|settings_service_smoke" --output-on-failure
cd apps/agent-core && go test ./...
```

Expected:
- CMake configure succeeds.
- Listed CTest tests pass.
- `go test ./...` passes.

- [ ] **Step 3: Confirm the authoritative design vocabulary**

Run:

```bash
rg -n "personaPrompt|ReplaceSummary|miles.chat.memory.summarizing|/v1/conversations|modernc.org/sqlite" docs/v2/设计方案/会话历史与人设设计.md
```

Expected: all five terms appear in the design document.

- [ ] **Step 4: No commit**

This task changes nothing.

---

## Task 1: Phase 2.3.2 Static Contract Scaffold

**Files:**
- Create: `tests/check_phase_2_3_2_session_persona.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing contract test**

Create `tests/check_phase_2_3_2_session_persona.py`:

```python
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


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    design = read("docs/v2/设计方案/会话历史与人设设计.md")
    provider_go = read("apps/agent-core/internal/chat/provider.go")
    openai_go = read("apps/agent-core/internal/chat/openai/provider.go")
    server_go = read("apps/agent-core/internal/api/server.go")
    main_go = read("apps/agent-core/cmd/miles-agent/main.go")
    store_go = read("apps/agent-core/internal/store/store.go")
    models_go = read("apps/agent-core/internal/models/catalog.go")
    service_go = read("apps/agent-core/internal/chat/service/service.go")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    persona_store_h = read("apps/desktop/src/pet/manifest/PersonaStore.h")
    settings_h = read("apps/desktop/src/settings/SettingsController.h")
    chat_h = read("apps/desktop/src/chat/ChatController.h")
    chat_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    settings_qml = read("apps/desktop/qml/SettingsWindow.qml")
    qrc = read("apps/desktop/resources/pet_assets.qrc")
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
    require("BuildSystemPrompt" not in openai_go, "OpenAI provider must not own system prompt construction")
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
    require("limit.context" in models_go, "models catalog must read limit.context")
    require("8192" in models_go, "models catalog must use 8192 fallback context window")

    require("type Service struct" in service_go, "ChatService must exist")
    require("BuildMessages" in service_go, "ChatService must build model messages")
    require("PersonaPrompt" in service_go or "personaPrompt" in service_go, "ChatService must use request persona")
    require("当前可用表达标签" in service_go, "ChatService must generate dynamic EXPR rules")
    require("以下是较早对话的摘要" in service_go, "summary must be injected as system context")
    require("EstimateTokens" in service_go and "len([]rune" in service_go, "token estimate must use rune count")
    require("miles.chat.memory.summarizing" in service_go, "ChatService/API must emit summarizing custom event")

    require('"/v1/conversations"' in server_go, "API must register conversations endpoint")
    require('"/v1/chat/messages"' in server_go, "chat endpoint must remain")
    require("AppendMessage" in server_go, "chat endpoint must persist user and assistant messages")
    require("isPartial" in server_go, "chat endpoint must expose partial messages")
    require("MILES_DATA_DIR" in main_go, "sidecar main must read MILES_DATA_DIR")
    require("store.Open" in main_go, "sidecar main must open SQLite store")

    require("personaPrompt" in manifest_h, "SkinManifest must contain personaPrompt")
    require("PersonaStore" in loader_cpp, "SkinManifestLoader must delegate persona read to PersonaStore")
    require("persona-overrides" in persona_store_h, "PersonaStore must support built-in override path")
    require("savePersona" in settings_h or "personaPrompt" in settings_h, "SettingsController must expose persona editing")
    require("角色人格" in settings_qml, "SettingsWindow must show persona editor")
    require("persona.md" in qrc, "Miles persona.md must be bundled")
    require("[EXPR:" not in persona, "persona.md must not contain EXPR marker instructions")

    require("conversations" in chat_h and "currentConversationId" in chat_h, "ChatController must expose conversations")
    require("loadConversations" in chat_h and "switchConversation" in chat_h, "ChatController must expose conversation methods")
    require("newConversation" in chat_h and "deleteConversation" in chat_h, "ChatController must expose new/delete methods")
    require("MILES_DATA_DIR" in chat_cpp, "ChatController must inject MILES_DATA_DIR to sidecar")
    require("personaPrompt" in chat_cpp, "ChatController must send current personaPrompt")
    require("conversationId" in chat_cpp and 'QStringLiteral("default")' not in chat_cpp, "ChatController must stop hardcoding default conversation id")
    require("miles.chat.memory.summarizing" in chat_cpp, "ChatController must handle summarizing event")
    require("会话" in chat_qml and "新建" in chat_qml, "ChatWindow must expose conversation list controls")

    require("PersonaStore" in desktop_cmake, "desktop CMake must compile PersonaStore")
    require("go.sum" in agent_cmake, "agent CMake must rebuild when go.sum changes")
    require("check_phase_2_3_2_session_persona" in root_cmake, "root CMake must register Phase 2.3.2 contract")
    require("Phase 2.3.2 会话历史与人设" in stage, "stage record must exist")
    require("会话历史与人设" in index, "doc index must link design/stage docs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Register the contract test in root CMake**

In root `CMakeLists.txt`, add this block after `check_phase_2_3_1_animation_sync`:

```cmake
        # Phase 2.3.2 的会话历史与人设检查：守住 persona.md、
        # SQLite 会话持久化、ChatService 上下文组装和会话列表 UI。
        add_test(
            NAME check_phase_2_3_2_session_persona
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_2_3_2_session_persona.py
        )
```

- [ ] **Step 3: Verify the new test fails**

Run:

```bash
python3 tests/check_phase_2_3_2_session_persona.py
```

Expected: FAIL with `missing file: apps/agent-core/internal/store/store.go`.

- [ ] **Step 4: Commit**

```bash
git add tests/check_phase_2_3_2_session_persona.py CMakeLists.txt
git commit -m "test: 增加 Phase 2.3.2 契约检查"
```

---

## Task 2: Go Provider Interface Migration

**Files:**
- Modify: `apps/agent-core/internal/chat/provider.go`
- Modify: `apps/agent-core/internal/chat/mock_provider.go`
- Modify: `apps/agent-core/internal/chat/openai/provider.go`
- Modify: `apps/agent-core/internal/chat/openai/provider_test.go`
- Modify: `apps/agent-core/internal/chat/expression/parser.go`
- Modify: `apps/agent-core/internal/chat/expression/parser_test.go`

- [ ] **Step 1: Write provider interface tests**

Update `apps/agent-core/internal/chat/openai/provider_test.go` so request-body tests construct prepared messages:

```go
params := chat.ChatParams{
    RunID:     "test-run",
    MessageID: "test-message",
    Messages: []chat.Message{
        {Role: "system", Content: "persona\n\nexpr rules"},
        {Role: "user", Content: "你好"},
    },
    KnownExpressionIDs: []string{"neutral", "objection", "polite"},
}
events, err := provider.StreamChat(context.Background(), params)
```

Assert the upstream JSON `messages` array is exactly:

```json
[
  {"role":"system","content":"persona\n\nexpr rules"},
  {"role":"user","content":"你好"}
]
```

Add a `Complete()` test with a fake OpenAI-compatible response:

```json
{"choices":[{"message":{"role":"assistant","content":"摘要文本"}}]}
```

Expected assertion: `Complete()` returns `摘要文本`.

- [ ] **Step 2: Run provider tests and confirm failure**

```bash
cd apps/agent-core
go test ./internal/chat/openai ./internal/chat/expression
```

Expected: compile failure because `StreamChat`, `Complete`, and `ChatParams` do not exist.

- [ ] **Step 3: Replace `chat.Provider` types**

Update `apps/agent-core/internal/chat/provider.go` to use this interface:

```go
package chat

import "context"

type ExpressionInfo struct {
    ID            string   `json:"id"`
    Label         string   `json:"label,omitempty"`
    Description   string   `json:"description,omitempty"`
    AllowedStates []string `json:"allowedStates,omitempty"`
}

type Request struct {
    ConversationID string           `json:"conversationId"`
    Message        string           `json:"message"`
    Expressions    []ExpressionInfo `json:"expressions,omitempty"`
    PersonaPrompt  string           `json:"personaPrompt,omitempty"`
}

type Message struct {
    Role    string `json:"role"`
    Content string `json:"content"`
}

type ChatParams struct {
    RunID              string
    MessageID          string
    Messages           []Message
    KnownExpressionIDs []string
}

type StreamEvent struct {
    Type      string         `json:"type"`
    Name      string         `json:"name,omitempty"`
    RunID     string         `json:"runId,omitempty"`
    MessageID string         `json:"messageId,omitempty"`
    Role      string         `json:"role,omitempty"`
    Delta     string         `json:"delta,omitempty"`
    Value     map[string]any `json:"value,omitempty"`
    Error     string         `json:"error,omitempty"`
}

type Provider interface {
    StreamChat(ctx context.Context, params ChatParams) (<-chan StreamEvent, error)
    Complete(ctx context.Context, params ChatParams) (string, error)
}
```

`RunID`, `MessageID`, and `KnownExpressionIDs` are local event metadata. Only `Messages` is serialized upstream, preserving the design rule that provider sends prepared model messages and does not assemble prompt/history.

- [ ] **Step 4: Update mock provider**

Change `MockProvider.StreamReply` to `StreamChat`. Build the mock reply from the last `role == "user"` message in `params.Messages`. Use `params.RunID` and `params.MessageID`, falling back to `mock-run-1` / `mock-message-1` only when empty. Add:

```go
func (p *MockProvider) Complete(ctx context.Context, params ChatParams) (string, error) {
    for i := len(params.Messages) - 1; i >= 0; i-- {
        if params.Messages[i].Role == "user" {
            return "摘要：" + params.Messages[i].Content, nil
        }
    }
    return "摘要：空对话", nil
}
```

- [ ] **Step 5: Update expression parser for metadata-free fallback**

Keep existing known-tag behavior. Add one rule: when `KnownTags` is empty, accept the parsed tag as-is unless it is empty. In `parser.go`:

```go
if p.OnExpression != nil {
    switch {
    case tag == "":
        p.OnExpression(p.FallbackTag)
    case len(p.known) == 0:
        p.OnExpression(tag)
    case p.known[tag]:
        p.OnExpression(tag)
    default:
        p.OnExpression(p.FallbackTag)
    }
}
```

Add a test:

```go
func TestParserAcceptsAllTagsWhenKnownListEmpty(t *testing.T) {
    cap := &capture{}
    p := newParser(cap, nil)
    p.Feed("[EXPR:objection]hi")
    p.Flush()
    if got := cap.string(); got != "E(objection)T(hi)" {
        t.Fatalf("got %q", got)
    }
}
```

- [ ] **Step 6: Update OpenAI provider**

Rename `StreamReply` to `StreamChat`. Remove `BuildSystemPrompt` and `buildSystemPrompt`. Convert `params.Messages` into the provider request body:

```go
messages := make([]chatMessage, 0, len(params.Messages))
for _, message := range params.Messages {
    messages = append(messages, chatMessage{Role: message.Role, Content: message.Content})
}
body := chatCompletionRequest{
    Model:       p.model,
    Messages:    messages,
    Stream:      true,
    Temperature: p.temperature,
    MaxTokens:   p.maxTokens,
}
```

Replace hard-coded run/message IDs with:

```go
runID := params.RunID
if runID == "" {
    runID = "openai-run-1"
}
messageID := params.MessageID
if messageID == "" {
    messageID = "openai-msg-1"
}
```

Change `pipe` signature:

```go
func (p *Provider) pipe(ctx context.Context, resp *http.Response, events chan<- chat.StreamEvent, runID, messageID string, knownTags []string)
```

Implement `Complete()` using the same endpoint with `Stream: false`, decode:

```go
type chatCompletionResponse struct {
    Choices []struct {
        Message chatMessage `json:"message"`
    } `json:"choices"`
}
```

Return the first non-empty `choices[0].message.content`, trimmed.

- [ ] **Step 7: Run provider tests**

```bash
cd apps/agent-core
go test ./internal/chat ./internal/chat/expression ./internal/chat/openai
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add apps/agent-core/internal/chat apps/agent-core/internal/chat/openai
git commit -m "feat: 升级聊天 Provider 消息接口"
```

---

## Task 3: SQLite Store Layer

**Files:**
- Create: `apps/agent-core/internal/store/store.go`
- Create: `apps/agent-core/internal/store/store_test.go`
- Modify: `apps/agent-core/go.mod`
- Modify: `apps/agent-core/go.sum`
- Modify: `apps/agent-core/CMakeLists.txt`

- [ ] **Step 1: Add dependency**

Run:

```bash
cd apps/agent-core
go get modernc.org/sqlite
go mod tidy
```

Expected: `go.mod` includes `modernc.org/sqlite`; `go.sum` is created or updated.

- [ ] **Step 2: Write store tests**

Create `apps/agent-core/internal/store/store_test.go` with tests covering:

```go
func TestStoreConversationLifecycle(t *testing.T) {
    s := openTestStore(t)
    conv, err := s.CreateConversation("miles-edgeworth")
    if err != nil { t.Fatal(err) }
    if conv.ID == "" || conv.SkinID != "miles-edgeworth" { t.Fatalf("bad conversation: %+v", conv) }
    user, err := s.AppendMessage(conv.ID, RoleUser, "你好", false)
    if err != nil { t.Fatal(err) }
    assistant, err := s.AppendMessage(conv.ID, RoleAssistant, "异议。", false)
    if err != nil { t.Fatal(err) }
    messages, err := s.GetMessages(conv.ID)
    if err != nil { t.Fatal(err) }
    if len(messages) != 2 || messages[0].ID != user.ID || messages[1].ID != assistant.ID {
        t.Fatalf("messages = %+v", messages)
    }
}
```

Add separate tests:
- `TestAppendMessageUpdatesConversationUpdatedAt`
- `TestDeleteConversationCascadesMessages`
- `TestReplaceSummaryIsTransactional`
- `TestUserAndSummaryCannotBePartial`
- `TestListConversationsHonorsLimitAndSortsByUpdatedAt`

Use helper:

```go
func openTestStore(t *testing.T) *Store {
    t.Helper()
    s, err := Open(t.TempDir())
    if err != nil { t.Fatal(err) }
    t.Cleanup(func() { _ = s.Close() })
    return s
}
```

- [ ] **Step 3: Run tests and confirm failure**

```bash
cd apps/agent-core
go test ./internal/store
```

Expected: FAIL because package `store` does not exist or has missing symbols.

- [ ] **Step 4: Implement store**

Create `apps/agent-core/internal/store/store.go` with:

```go
package store

import (
    "context"
    "crypto/rand"
    "database/sql"
    "encoding/hex"
    "errors"
    "fmt"
    "os"
    "path/filepath"
    "strings"

    _ "modernc.org/sqlite"
)

const (
    RoleUser      = "user"
    RoleAssistant = "assistant"
    RoleSummary   = "summary"
)

type Store struct { db *sql.DB }
type Conversation struct {
    ID string `json:"id"`
    Title string `json:"title"`
    SkinID string `json:"skinId"`
    CreatedAt string `json:"createdAt"`
    UpdatedAt string `json:"updatedAt"`
}
type Message struct {
    ID int64 `json:"id"`
    ConversationID string `json:"conversationId"`
    Role string `json:"role"`
    Content string `json:"content"`
    IsPartial bool `json:"isPartial"`
    CreatedAt string `json:"createdAt"`
}
var ErrNotFound = errors.New("not found")
```

Implement:
- `Open(dataDir string) (*Store, error)` creates `dataDir` with `0700`, opens `chat.db`, executes `PRAGMA foreign_keys = ON`, then schema.
- `newUUIDV4()` using `crypto/rand` and RFC 4122 version/variant bits.
- `CreateConversation(skinID string) (Conversation, error)`.
- `ListConversations(limit int) ([]Conversation, error)` with default `100` when `limit <= 0`.
- `GetConversation(id string) (Conversation, error)` returns `ErrNotFound` on no row.
- `DeleteConversation(id string) error`.
- `UpdateConversationTitle(id, title string) error`.
- `AppendMessage(conversationID, role, content string, partial bool) (Message, error)` in one transaction. If `role != RoleAssistant`, force `partial = false`. If this is the first user message and the title is empty, set title to the first 30 runes of content. Always update `conversations.updated_at`.
- `GetMessages(conversationID string) ([]Message, error)` ordered by `id ASC`.
- `ReplaceSummary(conversationID string, upToMessageID int64, summary string) error` in one transaction: delete messages with `id <= upToMessageID`, insert one `RoleSummary` row with `is_partial=0`, update `updated_at`.

Use this schema exactly:

```sql
CREATE TABLE IF NOT EXISTS conversations (
    id          TEXT PRIMARY KEY,
    title       TEXT NOT NULL DEFAULT '',
    skin_id     TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    updated_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE TABLE IF NOT EXISTS messages (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    conversation_id TEXT NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    role            TEXT NOT NULL,
    content         TEXT NOT NULL,
    is_partial      INTEGER NOT NULL DEFAULT 0,
    created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_messages_conversation ON messages(conversation_id, id);
```

- [ ] **Step 5: Make CMake rebuild on dependency checksum changes**

In `apps/agent-core/CMakeLists.txt`, add `go.sum` to the custom command dependencies:

```cmake
        "${CMAKE_CURRENT_SOURCE_DIR}/go.mod"
        "${CMAKE_CURRENT_SOURCE_DIR}/go.sum"
```

- [ ] **Step 6: Run store tests**

```bash
cd apps/agent-core
go test ./internal/store
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add apps/agent-core/internal/store apps/agent-core/go.mod apps/agent-core/go.sum apps/agent-core/CMakeLists.txt
git commit -m "feat: 增加 SQLite 会话存储"
```

---

## Task 4: models.dev Catalog

**Files:**
- Create: `apps/agent-core/internal/models/catalog.go`
- Create: `apps/agent-core/internal/models/catalog_test.go`

- [ ] **Step 1: Write catalog tests**

Create tests with an `httptest.Server` returning:

```json
{
  "openai": {
    "models": {
      "gpt-test": {
        "limit": {"context": 128000, "output": 4096},
        "modalities": {"input": ["text", "image"]}
      }
    }
  }
}
```

Test:
- `ContextWindow("gpt-test") == 128000`
- `SupportsVision("gpt-test") == true`
- unknown model returns `8192`
- cache file is written to `models-cache.json`
- stale cache is used when network fails.

- [ ] **Step 2: Run tests and confirm failure**

```bash
cd apps/agent-core
go test ./internal/models
```

Expected: FAIL because package is missing.

- [ ] **Step 3: Implement catalog**

Create `catalog.go` with:

```go
package models

import (
    "context"
    "encoding/json"
    "net/http"
    "os"
    "path/filepath"
    "time"
)

const DefaultContextWindow = 8192
const cacheFileName = "models-cache.json"
const defaultEndpoint = "https://models.dev/api.json"

type Catalog struct {
    dataDir string
    endpoint string
    client *http.Client
    models map[string]ModelInfo
}

type ModelInfo struct {
    ContextWindow int
    OutputLimit int
    SupportsVision bool
}
```

Expose:

```go
func NewCatalog(dataDir string) *Catalog
func NewCatalogWithClient(dataDir, endpoint string, client *http.Client) *Catalog
func (c *Catalog) Refresh(ctx context.Context) error
func (c *Catalog) ContextWindow(modelID string) int
func (c *Catalog) SupportsVision(modelID string) bool
```

Parse provider-grouped models by walking top-level provider objects, then `.models`, and mapping:
- `limit.context` → `ContextWindow`
- `limit.output` → `OutputLimit`
- `modalities.input` containing `"image"` → `SupportsVision`

Cache format:

```go
type cachePayload struct {
    FetchedAt time.Time `json:"fetchedAt"`
    Models map[string]ModelInfo `json:"models"`
}
```

On `Refresh`, load fresh cache if younger than 7 days. If stale, try network; on network error keep stale cache; if no cache, keep empty map and rely on default.

- [ ] **Step 4: Run tests**

```bash
cd apps/agent-core
go test ./internal/models
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/agent-core/internal/models
git commit -m "feat: 增加模型上下文窗口缓存"
```

---

## Task 5: ChatService Context Assembly and Summaries

**Files:**
- Create: `apps/agent-core/internal/chat/service/service.go`
- Create: `apps/agent-core/internal/chat/service/service_test.go`

- [ ] **Step 1: Write service tests**

Create tests for:
- Persona appears first in system prompt.
- EXPR rules are generated from request expressions and use original IDs.
- Summary DB rows are injected into system content, not as assistant messages.
- `EstimateTokens("你好") == 4`.
- Over-budget history emits `miles.chat.memory.summarizing` before provider call.
- `ReplaceSummary` is called with an `upToMessageID` that excludes current user and latest turn.
- Summary failure falls back to truncating older messages and preserves current user.

Use fake dependencies:

```go
type fakeProvider struct { completeText string; completeErr error; completeCalls int }
func (p *fakeProvider) StreamChat(context.Context, chat.ChatParams) (<-chan chat.StreamEvent, error) { panic("not used") }
func (p *fakeProvider) Complete(ctx context.Context, params chat.ChatParams) (string, error) {
    p.completeCalls++
    return p.completeText, p.completeErr
}
```

For store tests, use real `store.Store` from `t.TempDir()`.

- [ ] **Step 2: Run tests and confirm failure**

```bash
cd apps/agent-core
go test ./internal/chat/service
```

Expected: FAIL because package is missing.

- [ ] **Step 3: Implement service types**

Create `service.go` with:

```go
package service

import (
    "context"
    "fmt"
    "strings"

    "milesedgeworth/agent-core/internal/chat"
    "milesedgeworth/agent-core/internal/store"
)

const SummarizingEventName = "miles.chat.memory.summarizing"

type ModelCatalog interface {
    ContextWindow(modelID string) int
}

type Service struct {
    store *store.Store
    provider chat.Provider
    catalog ModelCatalog
    modelID string
}

type BuildRequest struct {
    ConversationID string
    PersonaPrompt string
    Expressions []chat.ExpressionInfo
    RunID string
    MessageID string
    Emit func(chat.StreamEvent) bool
}
```

Expose:

```go
func New(store *store.Store, provider chat.Provider, catalog ModelCatalog, modelID string) *Service
func EstimateTokens(text string) int
func (s *Service) BuildMessages(ctx context.Context, req BuildRequest) ([]chat.Message, []string, error)
func (s *Service) StreamChat(ctx context.Context, req BuildRequest) (<-chan chat.StreamEvent, error)
```

- [ ] **Step 4: Implement prompt assembly**

Implement `buildSystemPrompt(persona string, expressions []chat.ExpressionInfo, summaries []string) string`:

```go
var parts []string
if strings.TrimSpace(persona) != "" {
    parts = append(parts, strings.TrimSpace(persona))
}
parts = append(parts, buildExpressionRules(expressions))
if len(summaries) > 0 {
    parts = append(parts, "以下是较早对话的摘要：\n"+strings.Join(summaries, "\n\n"))
}
return strings.Join(parts, "\n\n")
```

`buildExpressionRules` must include:

```text
回复时在每段文字开头用 [EXPR:id] 标记当前表达。
只能使用方括号中列出的 id 原文，不要翻译 id，也不要使用中文 label。
例如使用 [EXPR:objection]，不要输出 [EXPR:异议]。
回复的第一段文字必须有标记。

当前可用表达标签：
- objection：强烈反驳
```

When expressions are empty, keep the format instructions and omit the list.

- [ ] **Step 5: Implement budget and summary flow**

`BuildMessages` reads `store.GetMessages(conversationID)`, splits summary rows from normal rows, builds system message, estimates tokens as:

```go
func EstimateTokens(text string) int { return len([]rune(text)) * 2 }
```

Use threshold:

```go
budget := s.catalog.ContextWindow(s.modelID)
if budget <= 0 { budget = 8192 }
limit := int(float64(budget) * 0.6)
```

If over budget:
- Find oldest normal messages eligible for compression.
- Exclude current user message and the latest prior turn.
- Emit:

```go
chat.StreamEvent{
    Type: "CUSTOM",
    Name: SummarizingEventName,
    RunID: req.RunID,
    Value: map[string]any{},
}
```

- Call `provider.Complete()` with summary prompt messages.
- On success call `store.ReplaceSummary(conversationID, upToMessageID, summary)`, then reload messages.
- On failure, drop only eligible old messages in memory until within budget.

Return:
- final `[]chat.Message` starting with exactly one `role:"system"` message.
- known expression IDs for the provider parser.

`StreamChat` calls `BuildMessages`, then calls the provider with local event metadata:

```go
return s.provider.StreamChat(ctx, chat.ChatParams{
    RunID: req.RunID,
    MessageID: req.MessageID,
    Messages: messages,
    KnownExpressionIDs: knownExpressionIDs,
})
```

- [ ] **Step 6: Run service tests**

```bash
cd apps/agent-core
go test ./internal/chat/service ./internal/store
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add apps/agent-core/internal/chat/service
git commit -m "feat: 增加聊天上下文组装服务"
```

---

## Task 6: Conversation API and Chat Persistence

**Files:**
- Modify: `apps/agent-core/internal/api/server.go`
- Modify: `apps/agent-core/internal/api/server_test.go`
- Modify: `apps/agent-core/cmd/miles-agent/main.go`

- [ ] **Step 1: Write API tests**

Update `server_test.go` to create a temp store and service. Cover:

```go
func TestConversationCRUD(t *testing.T) {
    srv := newTestServer(t)
    conv := postConversation(t, srv.URL, "miles-edgeworth")
    list := getConversations(t, srv.URL)
    if len(list) != 1 || list[0].ID != conv.ID { t.Fatalf("list = %+v", list) }
    deleteConversation(t, srv.URL, conv.ID)
    if got := getConversations(t, srv.URL); len(got) != 0 { t.Fatalf("after delete = %+v", got) }
}
```

Add tests:
- `TestChatPersistsUserAndAssistant`
- `TestChatRejectsUnknownConversation`
- `TestGetConversationMessagesIncludesPartialFlag`
- `TestChatPersistsPartialOnClientCancel`
- `TestSummarizingEventIsFirstSSEEventWhenTriggered`

- [ ] **Step 2: Run API tests and confirm failure**

```bash
cd apps/agent-core
go test ./internal/api
```

Expected: compile failure because `api.NewServer` still accepts only provider/label.

- [ ] **Step 3: Refactor server constructor**

Change server fields:

```go
type Server struct {
    store *store.Store
    chatService *service.Service
    providerLabel string
}

func NewServer(store *store.Store, chatService *service.Service, providerLabel string) *Server
```

Register routes:

```go
mux.HandleFunc("/health", s.handleHealth)
mux.HandleFunc("/v1/conversations", s.handleConversations)
mux.HandleFunc("/v1/conversations/", s.handleConversationByID)
mux.HandleFunc("/v1/chat/messages", s.handleChatMessages)
```

- [ ] **Step 4: Implement conversation handlers**

`GET /v1/conversations`:
- Parse `limit` as integer.
- Call `store.ListConversations(limit)`.
- Return `[{id,title,skinId,updatedAt}]`.

`POST /v1/conversations`:
- Decode `{"skinId":"..."}`.
- Call `store.CreateConversation`.
- Return status `201`.

`DELETE /v1/conversations/{id}`:
- Call `store.DeleteConversation`.
- Return `204`.

`GET /v1/conversations/{id}/messages`:
- Validate conversation exists.
- Return `[{id,role,content,isPartial,createdAt}]`.

- [ ] **Step 5: Implement persistence-aware chat handler**

Flow:
1. Decode `chat.Request`.
2. Trim and validate `Message`.
3. Validate `ConversationID` via `store.GetConversation`.
4. Append user message before SSE starts.
5. Generate run/message IDs in API:

```go
runID := "run-" + shortRandomID()
messageID := "msg-" + shortRandomID()
```

6. Write SSE headers and status `200`.
7. Call `chatService.StreamChat` with an `Emit` callback that writes SSE immediately during summary work.
8. Consume returned provider events; collect assistant deltas into a `strings.Builder`.
9. On normal `RUN_FINISHED`, append assistant full reply with `partial=false`.
10. On client disconnect or request context cancellation, append assistant builder text with `partial=true` when non-empty.
11. On provider `RUN_ERROR`, send error event and do not append assistant.

Keep the design invariant: `miles.chat.memory.summarizing` is emitted by ChatService after headers and before provider events.

- [ ] **Step 6: Update sidecar main**

In `main.go`:

```go
dataDir := os.Getenv("MILES_DATA_DIR")
if strings.TrimSpace(dataDir) == "" {
    dataDir = filepath.Join(os.TempDir(), "MilesEdgeworth")
}
st, err := store.Open(dataDir)
if err != nil { logger.Error("store open failed", "error", err); os.Exit(1) }
defer st.Close()

catalog := models.NewCatalog(dataDir)
if err := catalog.Refresh(context.Background()); err != nil {
    logger.Warn("models catalog refresh failed", "error", err)
}
chatService := service.New(st, provider, catalog, cfg.Model)
Handler: api.NewServer(st, chatService, label).Routes(),
```

Use `$MILES_DATA_DIR` when available; fallback only keeps local development functional.

- [ ] **Step 7: Run API and full Go tests**

```bash
cd apps/agent-core
go test ./...
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add apps/agent-core/internal/api apps/agent-core/cmd/miles-agent
git commit -m "feat: 增加会话 API 和聊天持久化"
```

---

## Task 7: Qt Persona Loading and Settings Editing

**Files:**
- Create: `apps/desktop/src/pet/manifest/PersonaStore.h`
- Create: `apps/desktop/src/pet/manifest/PersonaStore.cpp`
- Create: `apps/desktop/resources/skins/miles-edgeworth/persona.md`
- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/src/settings/SettingsController.h`
- Modify: `apps/desktop/src/settings/SettingsController.cpp`
- Modify: `apps/desktop/src/main.cpp`
- Modify: `apps/desktop/qml/SettingsWindow.qml`
- Modify: `apps/desktop/resources/pet_assets.qrc`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
- Modify: `apps/desktop/tests/settings_service_smoke.cpp`

- [ ] **Step 1: Write Qt tests**

Extend `skin_manifest_loader_smoke.cpp`:
- Create a temp filesystem skin with `persona.md`.
- Load it with `SkinManifestLoader::loadFromDirectory`.
- Assert `manifest.personaPrompt` equals file content.
- For built-in Miles, assert `manifest.personaPrompt` contains `Miles Edgeworth` or `御剑怜侍`.

Add a PersonaStore test block:
- Set `QStandardPaths::setTestModeEnabled(true)`.
- Save override for `miles-edgeworth`.
- Reload built-in manifest.
- Assert override wins.

Extend settings smoke:
- Construct `SettingsController settingsController(&settings, &runtime)`.
- Set `personaPrompt`.
- Call `save()`.
- Assert `runtime.reloadActiveSkin()` observes the saved persona.

- [ ] **Step 2: Run tests and confirm failure**

```bash
cmake --build build --target SkinManifestLoaderSmoke SettingsServiceSmoke
ctest --test-dir build -R "skin_manifest_loader_smoke|settings_service_smoke" --output-on-failure
```

Expected: compile failure because persona fields/helpers do not exist.

- [ ] **Step 3: Add `PersonaStore`**

Create `PersonaStore.h`:

```cpp
#pragma once

#include "pet/manifest/SkinDescriptor.h"
#include "pet/manifest/SkinManifest.h"

#include <QString>

class PersonaStore
{
public:
    static QString dataDir();
    static QString overridePathForSkin(const QString &skinId);
    static QString readForDescriptor(const SkinDescriptor &descriptor);
    static bool writeForManifest(const SkinManifest &manifest, const QString &content, QString *errorMessage);
};
```

Implementation rules:
- `dataDir()` returns `qEnvironmentVariable("MILES_DATA_DIR")` when non-empty, otherwise `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)`.
- Override path is `dataDir() + "/persona-overrides/" + skinId + ".md"`.
- Read priority: override path, then descriptor root `persona.md`, then empty.
- Built-in qrc read converts `qrc:/skins/miles-edgeworth/persona.md` to `:/skins/miles-edgeworth/persona.md`.
- Filesystem write target is `<skin root>/persona.md`.
- Built-in write target is override path. Create parent directories. Return `false` and an error string on failure.

- [ ] **Step 4: Add manifest field and loader integration**

In `SkinManifest.h`:

```cpp
QString personaPrompt;
```

In `SkinManifestLoader::loadFromDescriptor`, after parsing:

```cpp
manifest.personaPrompt = PersonaStore::readForDescriptor(descriptor);
```

In `fallbackManifest`, keep `personaPrompt` empty.

- [ ] **Step 5: Add Miles persona**

Create `apps/desktop/resources/skins/miles-edgeworth/persona.md` with Chinese role style. It must include:
- identity as Miles Edgeworth / 御剑怜侍
- prosecutor, precise reasoning, formal language
- reply in Chinese unless user asks otherwise
- avoid internet slang and overacting
- no instruction about `[EXPR:...]`

Add to `apps/desktop/resources/pet_assets.qrc` under prefix `/skins/miles-edgeworth`:

```xml
<file alias="persona.md">skins/miles-edgeworth/persona.md</file>
```

- [ ] **Step 6: Expose persona editing through SettingsController**

Change constructor:

```cpp
explicit SettingsController(SettingsService *service, PetRuntime *runtime = nullptr, QObject *parent = nullptr);
```

Add properties:

```cpp
Q_PROPERTY(QString personaPrompt READ personaPrompt WRITE setPersonaPrompt NOTIFY personaPromptChanged)
Q_PROPERTY(QString personaError READ personaError NOTIFY personaErrorChanged)
```

Add methods:

```cpp
QString personaPrompt() const { return m_personaPrompt; }
QString personaError() const { return m_personaError; }
void setPersonaPrompt(const QString &value);
Q_INVOKABLE void reloadPersona();
```

Behavior:
- `openWindow()` calls existing provider reload/revert plus `reloadPersona()`.
- `reloadPersona()` reads `m_runtime->manifest().personaPrompt` and clears error.
- `save()` first saves provider settings as before, then calls `PersonaStore::writeForManifest(m_runtime->manifest(), m_personaPrompt, &error)`.
- On persona save success, call `m_runtime->reloadActiveSkin()`, then `reloadPersona()`.
- On failure, set `personaError`, keep settings window open, and do not silently ignore the error.

- [ ] **Step 7: Add SettingsWindow persona editor**

In `SettingsWindow.qml`, add below provider grid:

```qml
Label {
    text: qsTr("角色人格")
    color: "#26201b"
    font.pixelSize: 16
    font.weight: Font.DemiBold
}

TextArea {
    id: personaField
    Layout.fillWidth: true
    Layout.preferredHeight: 180
    text: App.SettingsController.personaPrompt
    wrapMode: TextArea.Wrap
    selectByMouse: true
    placeholderText: qsTr("当前皮肤没有 persona.md，保存后会创建。")
    onTextEdited: App.SettingsController.personaPrompt = text
}

Label {
    visible: App.SettingsController.personaError.length > 0
    text: App.SettingsController.personaError
    color: "#b65a45"
    wrapMode: Text.WordWrap
    Layout.fillWidth: true
}
```

In the save button handler, set:

```qml
App.SettingsController.personaPrompt = personaField.text
```

- [ ] **Step 8: Wire CMake and main**

Add `PersonaStore.{h,cpp}` to `DESKTOP_SOURCES`, `SkinManifestLoaderSmoke`, and `ChatControllerSmoke` source lists.

In `main.cpp`:

```cpp
SettingsController settingsController(&settingsService, &petRuntime);
```

- [ ] **Step 9: Run Qt persona tests**

```bash
cmake --build build --target SkinManifestLoaderSmoke SettingsServiceSmoke MilesEdgeworthDesktop
ctest --test-dir build -R "skin_manifest_loader_smoke|settings_service_smoke" --output-on-failure
```

Expected: PASS.

- [ ] **Step 10: Commit**

```bash
git add apps/desktop/src/pet/manifest apps/desktop/src/settings apps/desktop/src/main.cpp apps/desktop/qml/SettingsWindow.qml apps/desktop/resources apps/desktop/CMakeLists.txt apps/desktop/tests
git commit -m "feat: 增加皮肤人设加载与编辑"
```

---

## Task 8: Qt Conversation Controller Integration

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Write controller smoke assertions**

Extend `chat_controller_smoke.cpp`:
- Assert `ChatController` exposes `conversations` and `currentConversationId` via meta-object.
- Apply a `CUSTOM` event named `miles.chat.memory.summarizing`; assert `statusText() == "整理记忆中..."` or `整理记忆中…`, matching chosen UI text.
- Keep request-body persona coverage in the Python contract and Go API tests; this C++ smoke focuses on properties and event handling.

- [ ] **Step 2: Run smoke and confirm failure**

```bash
cmake --build build --target ChatControllerSmoke
ctest --test-dir build -R chat_controller_smoke --output-on-failure
```

Expected: compile or assertion failure because properties/events do not exist.

- [ ] **Step 3: Add properties and invokables**

In `ChatController.h`:

```cpp
Q_PROPERTY(QVariantList conversations READ conversations NOTIFY conversationsChanged)
Q_PROPERTY(QString currentConversationId READ currentConversationId NOTIFY currentConversationIdChanged)
Q_PROPERTY(bool conversationSkinMismatch READ conversationSkinMismatch NOTIFY conversationSkinMismatchChanged)
Q_PROPERTY(QString conversationSkinHint READ conversationSkinHint NOTIFY conversationSkinMismatchChanged)
```

Add:

```cpp
QVariantList conversations() const { return m_conversations; }
QString currentConversationId() const { return m_currentConversationId; }
bool conversationSkinMismatch() const { return m_conversationSkinMismatch; }
QString conversationSkinHint() const { return m_conversationSkinHint; }

Q_INVOKABLE void loadConversations();
Q_INVOKABLE void switchConversation(const QString &id);
Q_INVOKABLE void newConversation();
Q_INVOKABLE void deleteConversation(const QString &id);
```

Add members:

```cpp
QVariantList m_conversations;
QString m_currentConversationId;
QString m_currentConversationSkinId;
bool m_conversationSkinMismatch = false;
QString m_conversationSkinHint;
```

- [ ] **Step 4: Add URLs and data-dir env**

In `ChatController.cpp` constants:

```cpp
constexpr auto kConversationsUrl = "http://127.0.0.1:39710/v1/conversations";
constexpr auto kMemorySummarizingEvent = "miles.chat.memory.summarizing";
```

In `launchSidecarProcess()`:

```cpp
const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
if (!dataDir.isEmpty()) {
    QDir().mkpath(dataDir);
    env.insert(QStringLiteral("MILES_DATA_DIR"), dataDir);
}
```

Add `dataDirSet` to debug log; never log the full path when not needed.

- [ ] **Step 5: Implement conversation loading**

`loadConversations()`:
- GET `/v1/conversations?limit=100`.
- Parse array.
- Add `isCurrent` to each item.
- Store to `m_conversations`.
- If no current conversation and list is not empty, call `switchConversation(first.id)`.

`switchConversation(id)`:
- Set current ID and matching skin ID.
- GET `/v1/conversations/{id}/messages`.
- Convert messages into `m_messages`, skipping `role == "summary"` for UI display.
- Map `content` to existing `text`, `isPartial` to a message flag.
- Update mismatch hint if conversation skin id differs from `m_runtime->manifest().skinId`.

`newConversation()`:
- Clear `m_currentConversationId`, `m_currentConversationSkinId`, `m_messages`.
- Clear mismatch state.

`deleteConversation(id)`:
- DELETE endpoint.
- Reload list.
- If deleted ID is current, call `newConversation()` before loading next list.

- [ ] **Step 6: Create conversation before send**

Refactor `sendMessage`:
- If `m_currentConversationId` is empty, POST `/v1/conversations` with current skin id.
- After creation response, set current ID, reload list, then call a private `sendMessageInConversation(trimmed)`.
- Existing `sendMessage` still appends the user and pending assistant to UI immediately after conversation ID is available.

Request body must include:

```cpp
body.insert(QStringLiteral("conversationId"), m_currentConversationId);
body.insert(QStringLiteral("message"), trimmed);
body.insert(QStringLiteral("personaPrompt"), m_runtime->manifest().personaPrompt);
body.insert(QStringLiteral("expressions"), expressionsArray);
```

Remove the hard-coded `"default"` conversation ID.

- [ ] **Step 7: Handle summarizing event**

In `applyStreamEvent` before expression handling:

```cpp
if (event.type == QStringLiteral("CUSTOM") && event.name == QString::fromLatin1(kMemorySummarizingEvent)) {
    setStatusText(QStringLiteral("整理记忆中..."));
    return;
}
```

When `RUN_STARTED` arrives, status returns to `正在回复` as it does today.

- [ ] **Step 8: Run controller smoke**

```bash
cmake --build build --target ChatControllerSmoke
ctest --test-dir build -R chat_controller_smoke --output-on-failure
```

Expected: PASS.

- [ ] **Step 9: Commit**

```bash
git add apps/desktop/src/chat apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "feat: 接入聊天会话控制器"
```

---

## Task 9: ChatWindow Conversation UI

**Files:**
- Modify: `apps/desktop/qml/ChatWindow.qml`

- [ ] **Step 1: Add conversation UI manually**

Modify `ChatWindow.qml`:
- Add a compact `ToolButton` at the left of the header with text `☰`.
- Add `property bool conversationPanelOpen: false`.
- Wrap transcript area in a `RowLayout`.
- Left panel width: 168 when open, 0 when closed.
- Panel contains:
  - `Button { text: "新建"; onClicked: App.ChatController.newConversation() }`
  - `ListView { model: App.ChatController.conversations }`
  - delegate with title, updatedAt, current highlight, delete button.
- On delegate click: `App.ChatController.switchConversation(modelData.id)`.
- Delete button: `App.ChatController.deleteConversation(modelData.id)`.
- Top of transcript shows a small warning strip when `App.ChatController.conversationSkinMismatch`.

Use existing restrained palette from the chat window:
- backgrounds `#f7f4ef`, `#fffdf8`, `#eee7da`
- borders `#d8d1c8`, `#bfae9e`
- text `#26201b`

- [ ] **Step 2: Keep message text selectable**

Do not replace the existing `TextEdit` message body with `Text`. Keep:

```qml
readOnly: true
selectByMouse: true
selectByKeyboard: true
textFormat: TextEdit.PlainText
```

Add partial display inside the delegate:

```qml
readonly property bool partial: modelData.isPartial === true
```

If partial is true, append a small muted label `已截断` below the bubble.

- [ ] **Step 3: Call `loadConversations` when window opens**

In `function open()`:

```qml
App.ChatController.loadConversations()
show()
raise()
requestActivate()
input.forceActiveFocus()
```

- [ ] **Step 4: Run QML-related build**

```bash
cmake --build build --target MilesEdgeworthDesktop
```

Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add apps/desktop/qml/ChatWindow.qml
git commit -m "feat: 增加聊天会话列表界面"
```

---

## Task 10: Documentation and Stage Record

**Files:**
- Create: `docs/v2/阶段记录/Phase 2.3.2 会话历史与人设.md`
- Modify: `docs/v2/文档索引.md`

- [ ] **Step 1: Write stage record**

Create `docs/v2/阶段记录/Phase 2.3.2 会话历史与人设.md`:

```markdown
# Phase 2.3.2 会话历史与人设

## 目标

本阶段落地 `docs/v2/设计方案/会话历史与人设设计.md`：皮肤人设、SQLite 会话历史、ChatService 上下文组装、摘要压缩和会话列表 UI。

## 已实现范围

- 皮肤 `persona.md` 加载与内置皮肤 override。
- 设置页角色人格编辑。
- sidecar `$MILES_DATA_DIR/chat.db` SQLite 会话存储。
- `/v1/conversations` 会话管理 API。
- `/v1/chat/messages` 使用真实 `conversationId` 并持久化 user/assistant。
- ChatService 拼装 persona、动态 EXPR 规则、摘要 system 上下文。
- 长对话触发摘要压缩，摘要中发送 `miles.chat.memory.summarizing`。
- ChatWindow 会话列表、新建、切换、删除。

## 验收

- `python3 tests/check_phase_2_3_2_session_persona.py`
- `cd apps/agent-core && go test ./...`
- `ctest --test-dir build -R "check_phase_2_3_2_session_persona|skin_manifest_loader_smoke|settings_service_smoke|chat_controller_smoke" --output-on-failure`

## 边界

- persona 始终跟随当前皮肤，sidecar 不缓存旧 persona。
- Provider 不读 store，不组装历史。
- 摘要作为 system prompt 内容，不作为 assistant 消息。
- Langfuse 可观测性留给 Phase 2.3.3。
```

- [ ] **Step 2: Update doc index**

In `docs/v2/文档索引.md`, add the stage record under Phase 2 records and ensure the design doc entry points to `设计方案/会话历史与人设设计.md`.

- [ ] **Step 3: Commit**

```bash
git add docs/v2/阶段记录/Phase\ 2.3.2\ 会话历史与人设.md docs/v2/文档索引.md
git commit -m "docs: 记录 Phase 2.3.2 会话历史与人设"
```

---

## Task 11: Full Verification and Contract Closure

**Files:**
- Read-only unless verification finds a defect in previous tasks.

- [ ] **Step 1: Run Python contracts**

```bash
python3 tests/check_phase_2_3_2_session_persona.py
python3 tests/check_phase_2_3_1_animation_sync.py
python3 tests/check_logging_standard.py
```

Expected: all PASS with no output.

- [ ] **Step 2: Run Go tests**

```bash
cd apps/agent-core
go test ./...
```

Expected: PASS for every Go package.

- [ ] **Step 3: Run CMake build and targeted CTest**

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target MilesEdgeworthDesktop
ctest --test-dir build -R "check_phase_2_3_2_session_persona|skin_manifest_loader_smoke|settings_service_smoke|chat_controller_smoke|chat_stream_event_parser_smoke|chat_text_pacer_smoke" --output-on-failure
```

Expected: build succeeds and tests pass.

- [ ] **Step 4: Manual local smoke**

Run from VS Code launch config or terminal:

```bash
MILES_LOG_LEVEL=debug MILES_LOG_PAYLOADS=0 ./build/apps/desktop/MilesEdgeworthDesktop.app/Contents/MacOS/MilesEdgeworthDesktop
```

Manual checks:
- Open chat window.
- Set provider config if needed.
- Send first message; app creates a conversation and persists messages.
- Quit app, relaunch, open chat; latest conversation and messages restore.
- Open settings, edit persona, save; next request sends changed persona without restart.
- Delete a conversation; it disappears and does not restore after relaunch.

- [ ] **Step 5: Check git status**

```bash
git status --short --branch
```

Expected: clean worktree.

- [ ] **Step 6: Final commit only if verification required small fixes**

If Step 1-4 required fixes, commit them:

```bash
git add <changed files>
git commit -m "fix: 收敛 Phase 2.3.2 验证问题"
```

Expected: clean worktree after commit.

---

## Self-Review Checklist for the Implementer

Before opening a PR or merging:
- [ ] `persona.md` contains no `[EXPR:` marker examples or instructions.
- [ ] Qt sends `personaPrompt` on every chat request.
- [ ] `ChatController.cpp` no longer contains `conversationId` hard-coded to `"default"`.
- [ ] Provider has no `BuildSystemPrompt` function and does not import `internal/store`.
- [ ] ChatService is the only layer that combines persona, dynamic EXPR rules, summaries, and normal history.
- [ ] `AppendMessage` and `ReplaceSummary` use transactions.
- [ ] `Store.Open` executes `PRAGMA foreign_keys = ON`.
- [ ] `messages.role == "summary"` is not displayed as a chat bubble.
- [ ] `miles.chat.memory.summarizing` is emitted after SSE headers and before provider `RUN_STARTED`.
- [ ] `MILES_DATA_DIR` is injected from Qt into sidecar.
- [ ] `chat.db`, `models-cache.json`, and persona overrides are written under app data, not the source tree.
- [ ] `python3 tests/check_phase_2_3_2_session_persona.py`, `go test ./...`, and targeted CTest all pass.
