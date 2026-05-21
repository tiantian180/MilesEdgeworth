# Phase 2.0 AI Chat MVP Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Revision notes (review 后修订)**：
>
> - **Task 4 ChatControllerForeign**：改用 `inline static` 与项目其他三个 Foreign 单例一致；删掉 .cpp 里的 out-of-line 定义。
> - **Task 4 manifest 三处修改**：从"给目标块"改成"完整 old_string → new_string 对照"，并在末尾加 `json.load` 校验，避免 search-and-replace 找错位置或写出非法 JSON。
> - **Task 4 ChatController 取消逻辑**：新增 `m_cancelled` flag。原版 `cancelCurrentReply` 调用 `finishCurrentReply` 后，残留 SSE chunks 仍会被 handleStreamBytes 处理，导致 `appendAssistantDelta` 因为 `m_assistantMessageIndex == -1` 新建一条"幽灵消息"。修订后 smoke 也加了回归断言。
> - **Task 5 PetSurfaceWindow 删除 Q_ASSERT(chatController != nullptr)**：Release 构建里 Q_ASSERT 是 no-op，提供假安全感。改为依赖菜单端的空指针检查；并加注释说明这是 Phase 2.0 内的临时耦合。
> - **Task 6 Step 5 多行 cd 链**：改成单行 `(cd ... && ...) && ...`，避免 agent shell 工具不维持工作目录时第二行 `cd ../..` 走偏。
>
> 原始 review 见 Phase 2.0 plan review 对话记录。

**Goal:** Build the Phase 2.0 AI Chat MVP skeleton: a mock Go sidecar streams chat events to a Qt/QML chat window, and the desktop pet reacts through the existing `PetRuntime::requestExpression(state, expression)` path.

**Architecture:** Keep the desktop pet body on the current QWidget `PetSurfaceWindow`; add a separate QML `ChatWindow` for chat UI. Add a local Go sidecar under `apps/agent-core` with `/health` and `POST /v1/chat/messages` SSE endpoints, then bridge it from C++ through `ChatController` using Qt Network and a small SSE parser. The sidecar emits AG-UI-shaped chat events plus `CUSTOM miles.pet.expression.requested`; Qt maps those custom events to `PetRuntime`, without teaching the skin runtime anything about model providers.

**Tech Stack:** Qt 6.5+ C++17, Qt Widgets, Qt Quick/QML, Qt Quick Controls, Qt Network, Go 1.22+ standard library, CMake/Ninja, CTest, existing Python static checks, existing C++ smoke tests.

---

## Scope Check

This plan implements **Phase 2.0: AI Chat MVP 骨架** only.

Included:
- `apps/agent-core` Go sidecar project.
- `/health` endpoint.
- Mock provider SSE stream for `POST /v1/chat/messages`.
- Event envelope for `RUN_STARTED`, `TEXT_MESSAGE_START`, `TEXT_MESSAGE_CONTENT`, `TEXT_MESSAGE_END`, `RUN_FINISHED`, `RUN_ERROR`, and `CUSTOM`.
- C++ SSE parser and `ChatController`.
- QML `ChatWindow` loaded by the desktop app.
- Native pet context menu entry to open chat.
- Expression state mapping: request starts as `thinking`, first streamed speech becomes `speaking`, errors become `error`, completion returns `idle`.
- Go tests, C++ parser/controller smoke tests, Python contract check, and CTest registration.

Excluded:
- Real OpenAI-compatible provider.
- API key, base URL, model settings UI, secure key storage.
- Conversation persistence and persona prompt.
- tools, skills, MCP, permissions, plugins, agent harness.
- Dashboard window.
- Pet Skin Studio, HitZone schema redesign, continuous scale slider.

## Assumptions

- Execution starts from `/Users/tian/projects/my-projects/MilesEdgeworth` on `main`.
- `legacy/v1-qt-widgets` is reference-only; do not merge or copy broad old code.
- The current machine does not have `go` installed. Before Task 2, install Go 1.22+ and verify `go version` succeeds. Do not implement a non-Go fallback sidecar.
- Phase 2.0 uses mock provider only. No code path calls external APIs or reads API keys.
- The sidecar is a local trusted process spawned by the desktop app. The HTTP server listens on `127.0.0.1` only.
- `PetRuntime::requestExpression()` is the only pet-expression bridge used by chat.

## File Structure

Create:
- `apps/agent-core/CMakeLists.txt`
  - Optional CMake wrapper that builds the Go sidecar when `go` is available.
- `apps/agent-core/go.mod`
  - Go module for sidecar.
- `apps/agent-core/cmd/miles-agent/main.go`
  - Sidecar executable entrypoint.
- `apps/agent-core/internal/api/server.go`
  - HTTP routes, health response, SSE writing.
- `apps/agent-core/internal/api/server_test.go`
  - Health and mock stream tests.
- `apps/agent-core/internal/chat/provider.go`
  - Provider interface and stream event structs.
- `apps/agent-core/internal/chat/mock_provider.go`
  - Deterministic mock streaming provider.
- `apps/desktop/src/chat/ChatStreamEvent.h`
  - Value type plus incremental SSE parser interface.
- `apps/desktop/src/chat/ChatStreamEvent.cpp`
  - SSE parsing and JSON envelope decoding.
- `apps/desktop/src/chat/ChatController.h`
  - QML-facing chat controller, sidecar process owner, network bridge.
- `apps/desktop/src/chat/ChatController.cpp`
  - Sidecar launch, health check, streaming request handling, expression mapping.
- `apps/desktop/qml/ChatWindow.qml`
  - Minimal chat UI.
- `apps/desktop/tests/chat_stream_event_parser_smoke.cpp`
  - C++ parser smoke test for split SSE chunks and custom pet events.
- `apps/desktop/tests/chat_controller_smoke.cpp`
  - C++ controller smoke test for message state and expression forwarding.
- `tests/check_phase_2_0_ai_chat_mvp.py`
  - Static contract check for the new skeleton.
- `docs/v2/阶段记录/Phase 2.0 AI Chat MVP 骨架.md`
  - Implementation record to add during the final task.

Modify:
- `CMakeLists.txt`
  - Add `apps/agent-core` subdirectory and register Phase 2.0 static check after implementation.
- `apps/desktop/CMakeLists.txt`
  - Add Qt Network and QuickControls2, chat sources, QML file, smoke tests, and sidecar copy step.
- `apps/desktop/src/main.cpp`
  - Instantiate `ChatController`, expose it to QML, load `ChatWindow.qml`, start sidecar health check.
- `apps/desktop/src/pet/surface/PetContextMenu.h`
  - Accept a `ChatController *`.
- `apps/desktop/src/pet/surface/PetContextMenu.cpp`
  - Add `聊天` action.
- `apps/desktop/src/pet/surface/PetSurfaceWindow.h`
  - Store a `ChatController *`.
- `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`
  - Pass controller into context menu.
- `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
  - Make `thinking` and `error` usable through the neutral expression mapping.
- `docs/v2/文档索引.md`
  - Add the Phase 2.0 stage record link.

## Task 0: Toolchain Preflight

**Files:**
- Read: `CMakeLists.txt`
- Read: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Verify branch and cleanliness**

Run:

```bash
git status --short --branch
```

Expected:

```text
## main...origin/main
```

If unrelated user changes exist, do not revert them. Continue only if they do not touch Phase 2.0 files.

- [ ] **Step 2: Verify Qt/CMake toolchain**

Run:

```bash
cmake --version | head -n 1
```

Expected: prints a CMake version. Current known local output is:

```text
cmake version 4.3.2
```

- [ ] **Step 3: Verify Go toolchain**

Run:

```bash
go version
```

Expected: PASS with Go 1.22 or newer, for example:

```text
go version go1.22.0 darwin/arm64
```

Current known local state is `zsh:1: command not found: go`. Install Go 1.22+ before Task 2, then rerun this step.

- [ ] **Step 4: Commit**

No files changed in this task.

## Task 1: Phase 2.0 Contract Test

**Files:**
- Create: `tests/check_phase_2_0_ai_chat_mvp.py`
- Modify later: `CMakeLists.txt`

- [ ] **Step 1: Write the static contract test**

Create `tests/check_phase_2_0_ai_chat_mvp.py`:

```python
#!/usr/bin/env python3
"""Check the Phase 2.0 AI chat MVP skeleton contract."""

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
    root_cmake = read("CMakeLists.txt")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    agent_cmake = read("apps/agent-core/CMakeLists.txt")
    go_mod = read("apps/agent-core/go.mod")
    server_go = read("apps/agent-core/internal/api/server.go")
    mock_provider_go = read("apps/agent-core/internal/chat/mock_provider.go")
    server_test = read("apps/agent-core/internal/api/server_test.go")
    parser_h = read("apps/desktop/src/chat/ChatStreamEvent.h")
    parser_cpp = read("apps/desktop/src/chat/ChatStreamEvent.cpp")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    main_cpp = read("apps/desktop/src/main.cpp")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    manifest = read("apps/desktop/resources/skins/miles-edgeworth/manifest.json")
    parser_smoke = read("apps/desktop/tests/chat_stream_event_parser_smoke.cpp")
    controller_smoke = read("apps/desktop/tests/chat_controller_smoke.cpp")
    phase_record = read("docs/v2/阶段记录/Phase 2.0 AI Chat MVP 骨架.md")
    index_doc = read("docs/v2/文档索引.md")

    require("add_subdirectory(apps/agent-core)" in root_cmake, "root CMake must include apps/agent-core")
    require("check_phase_2_0_ai_chat_mvp" in root_cmake, "root CMake must register Phase 2.0 check")

    for token in ["Network", "QuickControls2", "ChatController.cpp", "ChatStreamEvent.cpp", "ChatWindow.qml"]:
        require(token in desktop_cmake, f"desktop CMake missing {token}")

    require("go 1.22" in go_mod, "agent-core go.mod must target Go 1.22")
    require("find_program(GO_EXECUTABLE go)" in agent_cmake, "agent-core CMake must find go")
    require("/health" in server_go, "sidecar must expose /health")
    require("/v1/chat/messages" in server_go, "sidecar must expose chat messages endpoint")
    require("text/event-stream" in server_go, "chat endpoint must use SSE")
    require("RUN_STARTED" in mock_provider_go, "mock provider must emit RUN_STARTED")
    require("TEXT_MESSAGE_CONTENT" in mock_provider_go, "mock provider must emit token content")
    require("miles.pet.expression.requested" in mock_provider_go, "mock provider must emit pet expression custom events")
    require("httptest.NewServer" in server_test, "Go tests must cover HTTP server")

    for token in ["ChatStreamEvent", "ChatStreamEventParser", "ingest", "QJsonDocument"]:
        require(token in parser_h + parser_cpp, f"SSE parser missing {token}")

    for token in [
        "QNetworkAccessManager",
        "QProcess",
        "sendMessage",
        "cancelCurrentReply",
        "openWindowRequested",
        "requestExpression",
        "miles.pet.expression.requested",
    ]:
        require(token in controller_h + controller_cpp, f"ChatController missing {token}")

    require("import QtQuick.Controls" in chat_qml, "ChatWindow must use Qt Quick Controls")
    require("App.ChatController.sendMessage" in chat_qml, "ChatWindow must send through ChatController")
    require("onOpenWindowRequested" in chat_qml, "ChatWindow must react to controller open signal")
    require("ChatControllerForeign::s_instance" in main_cpp, "main must expose ChatController singleton")
    require("loadFromModule(\"MilesEdgeworth\", \"ChatWindow\")" in main_cpp, "main must load ChatWindow QML")
    require("聊天" in menu_cpp, "native pet context menu must include chat entry")
    require('"error"' in manifest, "Miles manifest must expose error state for chat failures")
    require('"action": "thinking", "allowedStates": ["thinking"]' in manifest, "neutral thinking must map to thinking action")
    require("miles.pet.expression.requested" in parser_smoke, "parser smoke must cover custom expression events")
    require("controller.applyStreamEvent" in controller_smoke, "controller smoke must cover event application")
    require("Phase 2.0" in phase_record and "mock provider" in phase_record, "phase record must document Phase 2.0")
    require("Phase 2.0 AI Chat MVP 骨架" in index_doc, "v2 index must link the Phase 2.0 record")

    print("phase 2.0 ai chat mvp contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Run the static test and verify it fails for missing Phase 2.0 files**

Run:

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
```

Expected: FAIL with `missing file: apps/agent-core/CMakeLists.txt`.

- [ ] **Step 3: Do not register this test yet**

Leave `CMakeLists.txt` unchanged in this task. The contract test covers the full Phase 2.0 skeleton, so registering it now would make every intermediate `ctest` fail.

- [ ] **Step 4: Commit**

Run:

```bash
git add tests/check_phase_2_0_ai_chat_mvp.py
git commit -m "test: add phase 2.0 ai chat contract"
```

Expected: commit succeeds.

## Task 2: Go Sidecar Mock API

**Files:**
- Create: `apps/agent-core/go.mod`
- Create: `apps/agent-core/cmd/miles-agent/main.go`
- Create: `apps/agent-core/internal/api/server.go`
- Create: `apps/agent-core/internal/api/server_test.go`
- Create: `apps/agent-core/internal/chat/provider.go`
- Create: `apps/agent-core/internal/chat/mock_provider.go`

- [ ] **Step 1: Write Go tests first**

Create `apps/agent-core/internal/api/server_test.go`:

```go
package api_test

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"milesedgeworth/agent-core/internal/api"
	"milesedgeworth/agent-core/internal/chat"
)

func TestHealth(t *testing.T) {
	server := httptest.NewServer(api.NewServer(chat.NewMockProvider(0)).Routes())
	defer server.Close()

	resp, err := http.Get(server.URL + "/health")
	if err != nil {
		t.Fatalf("GET /health failed: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}

	var body map[string]any
	if err := json.NewDecoder(resp.Body).Decode(&body); err != nil {
		t.Fatalf("decode health: %v", err)
	}
	if body["ok"] != true {
		t.Fatalf("health ok = %v, want true", body["ok"])
	}
	if body["provider"] != "mock" {
		t.Fatalf("provider = %v, want mock", body["provider"])
	}
}

func TestMockChatStream(t *testing.T) {
	server := httptest.NewServer(api.NewServer(chat.NewMockProvider(0)).Routes())
	defer server.Close()

	payload := []byte(`{"conversationId":"default","message":"hello miles"}`)
	resp, err := http.Post(server.URL+"/v1/chat/messages", "application/json", bytes.NewReader(payload))
	if err != nil {
		t.Fatalf("POST /v1/chat/messages failed: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}
	if got := resp.Header.Get("Content-Type"); !strings.Contains(got, "text/event-stream") {
		t.Fatalf("content-type = %q, want text/event-stream", got)
	}

	bodyBytes, err := io.ReadAll(resp.Body)
	if err != nil {
		t.Fatalf("read stream: %v", err)
	}
	body := string(bodyBytes)
	for _, token := range []string{
		`"type":"RUN_STARTED"`,
		`"name":"miles.pet.expression.requested"`,
		`"state":"thinking"`,
		`"type":"TEXT_MESSAGE_START"`,
		`"type":"TEXT_MESSAGE_CONTENT"`,
		`"type":"TEXT_MESSAGE_END"`,
		`"type":"RUN_FINISHED"`,
	} {
		if !strings.Contains(body, token) {
			t.Fatalf("stream missing %s in:\n%s", token, body)
		}
	}
}

func TestChatStreamRejectsEmptyMessage(t *testing.T) {
	server := httptest.NewServer(api.NewServer(chat.NewMockProvider(time.Millisecond)).Routes())
	defer server.Close()

	resp, err := http.Post(server.URL+"/v1/chat/messages", "application/json", strings.NewReader(`{"message":"   "}`))
	if err != nil {
		t.Fatalf("POST /v1/chat/messages failed: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusBadRequest {
		t.Fatalf("status = %d, want 400", resp.StatusCode)
	}
}
```

- [ ] **Step 2: Run Go tests and verify they fail because the module does not exist**

Run:

```bash
cd apps/agent-core && go test ./...
```

Expected: FAIL because `go.mod` and packages are missing.

- [ ] **Step 3: Add `go.mod`**

Create `apps/agent-core/go.mod`:

```go
module milesedgeworth/agent-core

go 1.22
```

- [ ] **Step 4: Add provider types**

Create `apps/agent-core/internal/chat/provider.go`:

```go
package chat

import "context"

type Request struct {
	ConversationID string
	Message        string
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
	StreamReply(ctx context.Context, req Request) (<-chan StreamEvent, error)
}
```

- [ ] **Step 5: Add deterministic mock provider**

Create `apps/agent-core/internal/chat/mock_provider.go`:

```go
package chat

import (
	"context"
	"fmt"
	"strings"
	"time"
)

type MockProvider struct {
	delay time.Duration
}

func NewMockProvider(delay time.Duration) *MockProvider {
	return &MockProvider{delay: delay}
}

func (p *MockProvider) StreamReply(ctx context.Context, req Request) (<-chan StreamEvent, error) {
	out := make(chan StreamEvent)
	message := strings.TrimSpace(req.Message)
	if message == "" {
		close(out)
		return out, nil
	}

	go func() {
		defer close(out)
		runID := "mock-run"
		messageID := "mock-assistant-message"

		events := []StreamEvent{
			{Type: "RUN_STARTED", RunID: runID},
			{
				Type: "CUSTOM",
				Name: "miles.pet.expression.requested",
				Value: map[string]any{
					"state":      "thinking",
					"expression": "neutral",
				},
			},
			{Type: "TEXT_MESSAGE_START", MessageID: messageID, Role: "assistant"},
			{
				Type: "CUSTOM",
				Name: "miles.pet.expression.requested",
				Value: map[string]any{
					"state":      "speaking",
					"expression": "objection",
				},
			},
		}

		for _, event := range events {
			if !send(ctx, out, event, p.delay) {
				return
			}
		}

		reply := fmt.Sprintf("异议。你刚才说的是：%s。Phase 2.0 mock 链路已经接通。", message)
		for _, token := range splitReply(reply) {
			if !send(ctx, out, StreamEvent{Type: "TEXT_MESSAGE_CONTENT", MessageID: messageID, Delta: token}, p.delay) {
				return
			}
		}

		for _, event := range []StreamEvent{
			{Type: "TEXT_MESSAGE_END", MessageID: messageID},
			{
				Type: "CUSTOM",
				Name: "miles.pet.expression.requested",
				Value: map[string]any{
					"state":      "idle",
					"expression": "neutral",
				},
			},
			{Type: "RUN_FINISHED", RunID: runID},
		} {
			if !send(ctx, out, event, p.delay) {
				return
			}
		}
	}()

	return out, nil
}

func splitReply(reply string) []string {
	words := strings.Fields(reply)
	if len(words) == 0 {
		return []string{reply}
	}

	tokens := make([]string, 0, len(words))
	for i, word := range words {
		if i == 0 {
			tokens = append(tokens, word)
			continue
		}
		tokens = append(tokens, " "+word)
	}
	return tokens
}

func send(ctx context.Context, out chan<- StreamEvent, event StreamEvent, delay time.Duration) bool {
	if delay > 0 {
		timer := time.NewTimer(delay)
		select {
		case <-ctx.Done():
			timer.Stop()
			return false
		case <-timer.C:
		}
	}

	select {
	case <-ctx.Done():
		return false
	case out <- event:
		return true
	}
}
```

- [ ] **Step 6: Add HTTP server**

Create `apps/agent-core/internal/api/server.go`:

```go
package api

import (
	"encoding/json"
	"errors"
	"log/slog"
	"net/http"
	"strings"

	"milesedgeworth/agent-core/internal/chat"
)

type Server struct {
	provider chat.Provider
}

type chatRequest struct {
	ConversationID string `json:"conversationId"`
	Message        string `json:"message"`
}

func NewServer(provider chat.Provider) *Server {
	return &Server{provider: provider}
}

func (s *Server) Routes() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/health", s.handleHealth)
	mux.HandleFunc("/v1/chat/messages", s.handleChatMessages)
	return mux
}

func (s *Server) handleHealth(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, map[string]any{
		"ok":       true,
		"service":  "miles-agent",
		"provider": "mock",
	})
}

func (s *Server) handleChatMessages(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var req chatRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "invalid json", http.StatusBadRequest)
		return
	}

	req.Message = strings.TrimSpace(req.Message)
	if req.Message == "" {
		http.Error(w, "message is required", http.StatusBadRequest)
		return
	}
	if req.ConversationID == "" {
		req.ConversationID = "default"
	}

	stream, err := s.provider.StreamReply(r.Context(), chat.Request{
		ConversationID: req.ConversationID,
		Message:        req.Message,
	})
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}

	w.Header().Set("Content-Type", "text/event-stream; charset=utf-8")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")

	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "streaming unsupported", http.StatusInternalServerError)
		return
	}

	for event := range stream {
		if err := writeSSE(w, event); err != nil {
			if !errors.Is(err, http.ErrHandlerTimeout) {
				slog.Debug("write sse failed", "error", err)
			}
			return
		}
		flusher.Flush()
	}
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}

func writeSSE(w http.ResponseWriter, event chat.StreamEvent) error {
	payload, err := json.Marshal(event)
	if err != nil {
		return err
	}
	_, err = w.Write(append(append([]byte("data: "), payload...), '\n', '\n'))
	return err
}
```

- [ ] **Step 7: Add sidecar executable**

Create `apps/agent-core/cmd/miles-agent/main.go`:

```go
package main

import (
	"context"
	"errors"
	"flag"
	"log/slog"
	"net"
	"net/http"
	"os"
	"os/signal"
	"time"

	"milesedgeworth/agent-core/internal/api"
	"milesedgeworth/agent-core/internal/chat"
)

func main() {
	addr := flag.String("addr", "127.0.0.1:39710", "HTTP listen address")
	flag.Parse()

	listener, err := net.Listen("tcp", *addr)
	if err != nil {
		slog.Error("listen failed", "addr", *addr, "error", err)
		os.Exit(1)
	}

	server := &http.Server{
		Handler:           api.NewServer(chat.NewMockProvider(35 * time.Millisecond)).Routes(),
		ReadHeaderTimeout: 5 * time.Second,
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt)
	defer stop()

	go func() {
		<-ctx.Done()
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()
		_ = server.Shutdown(shutdownCtx)
	}()

	slog.Info("miles agent listening", "addr", listener.Addr().String())
	if err := server.Serve(listener); err != nil && !errors.Is(err, http.ErrServerClosed) {
		slog.Error("server failed", "error", err)
		os.Exit(1)
	}
}
```

- [ ] **Step 8: Run Go tests**

Run:

```bash
cd apps/agent-core && go test ./...
```

Expected: PASS.

- [ ] **Step 9: Run sidecar manually and verify health**

Run in terminal A:

```bash
cd apps/agent-core && go run ./cmd/miles-agent -addr 127.0.0.1:39710
```

Run in terminal B:

```bash
curl -s http://127.0.0.1:39710/health
```

Expected body contains:

```text
"ok":true
"provider":"mock"
"service":"miles-agent"
```

Stop terminal A with `Ctrl-C`.

- [ ] **Step 10: Commit**

Run:

```bash
git add apps/agent-core
git commit -m "feat: add mock agent sidecar"
```

Expected: commit succeeds.

## Task 3: Desktop SSE Parser

**Files:**
- Create: `apps/desktop/src/chat/ChatStreamEvent.h`
- Create: `apps/desktop/src/chat/ChatStreamEvent.cpp`
- Create: `apps/desktop/tests/chat_stream_event_parser_smoke.cpp`
- Modify later: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Write parser smoke test**

Create `apps/desktop/tests/chat_stream_event_parser_smoke.cpp`:

```cpp
#include "chat/ChatStreamEvent.h"

#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    ChatStreamEventParser parser;
    QList<ChatStreamEvent> events = parser.ingest(
        "data: {\"type\":\"RUN_STARTED\",\"runId\":\"mock-run\"}\n\n"
        "data: {\"type\":\"TEXT_MESSAGE_START\",\"messageId\":\"a1\",\"role\":\"assistant\"}\n\n"
    );

    require(events.size() == 2, "two complete events should parse");
    require(events[0].type == "RUN_STARTED", "first event type should be RUN_STARTED");
    require(events[0].runId == "mock-run", "runId should parse");
    require(events[1].type == "TEXT_MESSAGE_START", "second event type should be TEXT_MESSAGE_START");
    require(events[1].messageId == "a1", "messageId should parse");
    require(events[1].role == "assistant", "role should parse");

    events = parser.ingest("data: {\"type\":\"TEXT_MESSAGE_CONTENT\",\"messageId\":\"a1\",\"delta\":\"异");
    require(events.isEmpty(), "partial event should not parse");
    events = parser.ingest("议\"}\n\n");
    require(events.size() == 1, "partial event should complete after second chunk");
    require(events[0].type == "TEXT_MESSAGE_CONTENT", "content event should parse");
    require(events[0].delta == QStringLiteral("异议"), "delta should preserve unicode text");

    events = parser.ingest(
        "event: message\n"
        "data: {\"type\":\"CUSTOM\",\"name\":\"miles.pet.expression.requested\","
        "\"value\":{\"state\":\"speaking\",\"expression\":\"objection\"}}\n\n"
    );
    require(events.size() == 1, "custom event should parse");
    require(events[0].type == "CUSTOM", "custom type should parse");
    require(events[0].name == "miles.pet.expression.requested", "custom name should parse");
    require(events[0].value.value("state").toString() == "speaking", "custom state should parse");
    require(events[0].value.value("expression").toString() == "objection", "custom expression should parse");

    return 0;
}
```

- [ ] **Step 2: Add parser header**

Create `apps/desktop/src/chat/ChatStreamEvent.h`:

```cpp
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantMap>

struct ChatStreamEvent
{
    QString type;
    QString name;
    QString runId;
    QString messageId;
    QString role;
    QString delta;
    QString error;
    QVariantMap value;
};

class ChatStreamEventParser
{
public:
    QList<ChatStreamEvent> ingest(const QByteArray &chunk);

private:
    QByteArray m_buffer;
};
```

- [ ] **Step 3: Add parser implementation**

Create `apps/desktop/src/chat/ChatStreamEvent.cpp`:

```cpp
#include "chat/ChatStreamEvent.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>

namespace {
ChatStreamEvent eventFromObject(const QJsonObject &object)
{
    ChatStreamEvent event;
    event.type = object.value(QStringLiteral("type")).toString();
    event.name = object.value(QStringLiteral("name")).toString();
    event.runId = object.value(QStringLiteral("runId")).toString();
    event.messageId = object.value(QStringLiteral("messageId")).toString();
    event.role = object.value(QStringLiteral("role")).toString();
    event.delta = object.value(QStringLiteral("delta")).toString();
    event.error = object.value(QStringLiteral("error")).toString();
    event.value = object.value(QStringLiteral("value")).toObject().toVariantMap();
    return event;
}

QByteArray dataPayloadFromFrame(const QByteArray &frame)
{
    QByteArray data;
    const QList<QByteArray> lines = frame.split('\n');
    for (QByteArray line : lines) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (!line.startsWith("data:")) {
            continue;
        }

        QByteArray value = line.mid(5);
        if (value.startsWith(' ')) {
            value.remove(0, 1);
        }
        if (!data.isEmpty()) {
            data.append('\n');
        }
        data.append(value);
    }
    return data;
}
} // namespace

QList<ChatStreamEvent> ChatStreamEventParser::ingest(const QByteArray &chunk)
{
    m_buffer.append(chunk);

    QList<ChatStreamEvent> events;
    while (true) {
        int delimiter = m_buffer.indexOf("\n\n");
        int delimiterLength = 2;
        const int crlfDelimiter = m_buffer.indexOf("\r\n\r\n");
        if (crlfDelimiter >= 0 && (delimiter < 0 || crlfDelimiter < delimiter)) {
            delimiter = crlfDelimiter;
            delimiterLength = 4;
        }

        if (delimiter < 0) {
            break;
        }

        const QByteArray frame = m_buffer.left(delimiter);
        m_buffer.remove(0, delimiter + delimiterLength);
        const QByteArray payload = dataPayloadFromFrame(frame);
        if (payload.trimmed().isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }

        const ChatStreamEvent event = eventFromObject(document.object());
        if (!event.type.isEmpty()) {
            events.append(event);
        }
    }

    return events;
}
```

- [ ] **Step 4: Temporarily wire the parser test locally**

Add these entries to `apps/desktop/CMakeLists.txt` under `if(BUILD_TESTING)`:

```cmake
    add_executable(ChatStreamEventParserSmoke
        tests/chat_stream_event_parser_smoke.cpp
        src/chat/ChatStreamEvent.cpp
        src/chat/ChatStreamEvent.h
    )

    target_include_directories(ChatStreamEventParserSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ChatStreamEventParserSmoke
        PRIVATE
            Qt6::Core
    )

    add_test(NAME chat_stream_event_parser_smoke COMMAND ChatStreamEventParserSmoke)
```

- [ ] **Step 5: Build and run parser test**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target ChatStreamEventParserSmoke
ctest --test-dir build -R chat_stream_event_parser_smoke --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

Run:

```bash
git add apps/desktop/src/chat apps/desktop/tests/chat_stream_event_parser_smoke.cpp apps/desktop/CMakeLists.txt
git commit -m "feat: add chat stream event parser"
```

Expected: commit succeeds.

## Task 4: ChatController State Bridge

**Files:**
- Create: `apps/desktop/src/chat/ChatController.h`
- Create: `apps/desktop/src/chat/ChatController.cpp`
- Create: `apps/desktop/tests/chat_controller_smoke.cpp`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`

- [ ] **Step 1: Write controller smoke test**

Create `apps/desktop/tests/chat_controller_smoke.cpp`:

```cpp
#include "chat/ChatController.h"
#include "chat/ChatStreamEvent.h"
#include "pet/PetRuntime.h"

#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    PetRuntime runtime;
    ChatController controller(&runtime);

    ChatStreamEvent started;
    started.type = QStringLiteral("RUN_STARTED");
    started.runId = QStringLiteral("mock-run");
    controller.applyStreamEvent(started);
    require(controller.sending(), "RUN_STARTED should mark controller as sending");

    ChatStreamEvent thinkingEvent;
    thinkingEvent.type = QStringLiteral("CUSTOM");
    thinkingEvent.name = QStringLiteral("miles.pet.expression.requested");
    thinkingEvent.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
    thinkingEvent.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
    controller.applyStreamEvent(thinkingEvent);
    require(runtime.currentState() == QStringLiteral("thinking"), "custom thinking event should update runtime state");
    require(runtime.currentActionId() == QStringLiteral("thinking"), "thinking neutral should play thinking action");

    ChatStreamEvent messageStart;
    messageStart.type = QStringLiteral("TEXT_MESSAGE_START");
    messageStart.messageId = QStringLiteral("assistant-1");
    messageStart.role = QStringLiteral("assistant");
    controller.applyStreamEvent(messageStart);
    require(controller.messages().size() == 1, "assistant message should be appended");

    ChatStreamEvent messageContent;
    messageContent.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
    messageContent.messageId = QStringLiteral("assistant-1");
    messageContent.delta = QStringLiteral("异议");
    controller.applyStreamEvent(messageContent);
    require(controller.messages().constFirst().toMap().value("text").toString() == QStringLiteral("异议"),
            "content delta should append to assistant message");

    ChatStreamEvent expressionEvent;
    expressionEvent.type = QStringLiteral("CUSTOM");
    expressionEvent.name = QStringLiteral("miles.pet.expression.requested");
    expressionEvent.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
    expressionEvent.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
    controller.applyStreamEvent(expressionEvent);
    require(runtime.currentState() == QStringLiteral("speaking"), "custom expression event should update runtime state");
    require(runtime.currentActionId() == QStringLiteral("objecting"), "speaking objection should play objecting action");

    ChatStreamEvent finished;
    finished.type = QStringLiteral("RUN_FINISHED");
    finished.runId = QStringLiteral("mock-run");
    controller.applyStreamEvent(finished);
    require(!controller.sending(), "RUN_FINISHED should clear sending");
    require(runtime.currentState() == QStringLiteral("idle"), "RUN_FINISHED should return pet to idle");

    ChatStreamEvent errorEvent;
    errorEvent.type = QStringLiteral("RUN_ERROR");
    errorEvent.error = QStringLiteral("mock failure");
    controller.applyStreamEvent(errorEvent);
    require(runtime.currentState() == QStringLiteral("error"), "RUN_ERROR should move pet to error state");

    // 取消后到达的残留事件不应该新建 assistant 消息
    controller.sendMessage(QStringLiteral("again"));
    const int messagesBeforeCancel = controller.messages().size();
    controller.cancelCurrentReply();
    require(!controller.sending(), "cancel should clear sending");

    ChatStreamEvent strayContent;
    strayContent.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
    strayContent.messageId = QStringLiteral("assistant-2");
    strayContent.delta = QStringLiteral("残留 token");
    controller.applyStreamEvent(strayContent);
    require(controller.messages().size() == messagesBeforeCancel,
            "stream content after cancel must not append a new assistant message");

    return 0;
}
```

- [ ] **Step 2: Add controller header**

Create `apps/desktop/src/chat/ChatController.h`:

```cpp
#pragma once

#include "chat/ChatStreamEvent.h"

#include <QJSEngine>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QNetworkReply;
class PetRuntime;

class ChatController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool sidecarReady READ sidecarReady NOTIFY sidecarReadyChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY sendingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)

public:
    explicit ChatController(PetRuntime *runtime, QObject *parent = nullptr);
    ~ChatController() override;

    bool sidecarReady() const { return m_sidecarReady; }
    bool sending() const { return m_sending; }
    QString statusText() const { return m_statusText; }
    QVariantList messages() const { return m_messages; }

    Q_INVOKABLE void openWindow();
    Q_INVOKABLE void startSidecar();
    Q_INVOKABLE void checkHealth();
    Q_INVOKABLE void sendMessage(const QString &message);
    Q_INVOKABLE void cancelCurrentReply();

    void applyStreamEvent(const ChatStreamEvent &event);

signals:
    void sidecarReadyChanged();
    void sendingChanged();
    void statusTextChanged();
    void messagesChanged();
    void openWindowRequested();

private:
    QVariantMap messageObject(const QString &role, const QString &text, bool pending, bool error) const;
    void appendMessage(const QVariantMap &message);
    void appendAssistantDelta(const QString &delta);
    void setSidecarReady(bool ready);
    void setSending(bool sending);
    void setStatusText(const QString &statusText);
    void requestPetExpression(const QString &state, const QString &expression);
    QString sidecarExecutablePath() const;
    void handleStreamBytes(const QByteArray &bytes);
    void finishCurrentReply();
    void failCurrentReply(const QString &message);

    PetRuntime *m_runtime = nullptr;
    QNetworkAccessManager m_network;
    QProcess m_sidecarProcess;
    QPointer<QNetworkReply> m_currentReply;
    ChatStreamEventParser m_parser;
    QVariantList m_messages;
    bool m_sidecarReady = false;
    bool m_sending = false;
    // 用户取消后，剩余 SSE chunks 必须被丢弃，否则会拼到新建的 assistant 消息里产生"幽灵回复"。
    // 每次 sendMessage 复位 false。
    bool m_cancelled = false;
    QString m_statusText = QStringLiteral("未连接");
    int m_assistantMessageIndex = -1;
};

struct ChatControllerForeign
{
    Q_GADGET
    QML_FOREIGN(ChatController)
    QML_NAMED_ELEMENT(ChatController)
    QML_SINGLETON

public:
    // 与 PetRuntimeForeign / PetEventBridgeForeign / DesktopShellControllerForeign 保持一致，使用 inline static 就地定义。
    inline static ChatController *s_instance = nullptr;

    static ChatController *create(QQmlEngine *, QJSEngine *)
    {
        return s_instance;
    }
};
```

- [ ] **Step 3: Add controller implementation**

Create `apps/desktop/src/chat/ChatController.cpp`:

```cpp
#include "chat/ChatController.h"

#include "pet/PetRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QUrl>

namespace {
constexpr auto kHealthUrl = "http://127.0.0.1:39710/health";
constexpr auto kChatUrl = "http://127.0.0.1:39710/v1/chat/messages";
constexpr auto kExpressionEventName = "miles.pet.expression.requested";

QString executableName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("miles-agent.exe");
#else
    return QStringLiteral("miles-agent");
#endif
}
} // namespace

// 注意：ChatControllerForeign::s_instance 已经在 .h 中以 inline static 形式定义，
// 此处不需要再写 out-of-line 定义，否则会触发重复定义。

ChatController::ChatController(PetRuntime *runtime, QObject *parent)
    : QObject(parent)
    , m_runtime(runtime)
{
    Q_ASSERT(m_runtime != nullptr);

    connect(&m_sidecarProcess, &QProcess::started, this, [this]() {
        setStatusText(QStringLiteral("正在连接"));
        QTimer::singleShot(200, this, &ChatController::checkHealth);
    });
    connect(&m_sidecarProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        setSidecarReady(false);
        setStatusText(QStringLiteral("sidecar 启动失败"));
    });
    connect(&m_sidecarProcess, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        setSidecarReady(false);
        if (m_sending) {
            failCurrentReply(QStringLiteral("sidecar 已退出"));
        } else {
            setStatusText(QStringLiteral("sidecar 已退出"));
        }
    });
}

ChatController::~ChatController()
{
    cancelCurrentReply();
    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        m_sidecarProcess.terminate();
        if (!m_sidecarProcess.waitForFinished(1500)) {
            m_sidecarProcess.kill();
            m_sidecarProcess.waitForFinished(500);
        }
    }
}

void ChatController::openWindow()
{
    emit openWindowRequested();
}

void ChatController::startSidecar()
{
    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        checkHealth();
        return;
    }

    const QString path = sidecarExecutablePath();
    if (path.isEmpty()) {
        setSidecarReady(false);
        setStatusText(QStringLiteral("找不到 miles-agent"));
        return;
    }

    m_sidecarProcess.start(path, {QStringLiteral("-addr"), QStringLiteral("127.0.0.1:39710")});
}

void ChatController::checkHealth()
{
    QNetworkReply *reply = m_network.get(QNetworkRequest(QUrl(QString::fromUtf8(kHealthUrl))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError;
        setSidecarReady(ok);
        setStatusText(ok ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    });
}

void ChatController::sendMessage(const QString &message)
{
    const QString trimmed = message.trimmed();
    if (trimmed.isEmpty() || m_sending) {
        return;
    }

    m_cancelled = false;  // 新一轮发送：清掉上一轮可能残留的取消标记
    appendMessage(messageObject(QStringLiteral("user"), trimmed, false, false));
    appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
    m_assistantMessageIndex = m_messages.size() - 1;
    setSending(true);
    setStatusText(QStringLiteral("思考中"));
    requestPetExpression(QStringLiteral("thinking"), QStringLiteral("neutral"));

    QJsonObject body;
    body.insert(QStringLiteral("conversationId"), QStringLiteral("default"));
    body.insert(QStringLiteral("message"), trimmed);

    QNetworkRequest request(QUrl(QString::fromUtf8(kChatUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    m_currentReply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_currentReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_currentReply) {
            handleStreamBytes(m_currentReply->readAll());
        }
    });
    connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
        if (!m_currentReply) {
            return;
        }

        const QNetworkReply::NetworkError error = m_currentReply->error();
        const QString errorText = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;

        if (error != QNetworkReply::NoError) {
            failCurrentReply(errorText);
            return;
        }
        finishCurrentReply();
    });
}

void ChatController::cancelCurrentReply()
{
    if (!m_currentReply && !m_sending) {
        return;
    }

    // 标记取消，丢弃后续可能到达的 SSE chunks；不立刻 finish，等 reply 的 finished 信号顺其自然走完一遍清理。
    m_cancelled = true;
    if (m_currentReply) {
        m_currentReply->abort();
    }

    // 把当前 assistant 消息收尾（pending=false，不标 error），让 UI 立即停止 spinner。
    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap message = m_messages[m_assistantMessageIndex].toMap();
        message.insert(QStringLiteral("pending"), false);
        m_messages[m_assistantMessageIndex] = message;
        emit messagesChanged();
    }
    m_assistantMessageIndex = -1;
    setSending(false);
    setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    requestPetExpression(QStringLiteral("idle"), QStringLiteral("neutral"));
}

void ChatController::applyStreamEvent(const ChatStreamEvent &event)
{
    if (m_cancelled) {
        // 取消后到达的残留事件直接丢弃，避免拼到 m_assistantMessageIndex == -1 的"幽灵新消息"。
        return;
    }

    if (event.type == QStringLiteral("RUN_STARTED")) {
        setSending(true);
        setStatusText(QStringLiteral("思考中"));
        return;
    }

    if (event.type == QStringLiteral("TEXT_MESSAGE_START")) {
        if (m_assistantMessageIndex < 0) {
            appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
            m_assistantMessageIndex = m_messages.size() - 1;
        }
        return;
    }

    if (event.type == QStringLiteral("TEXT_MESSAGE_CONTENT")) {
        appendAssistantDelta(event.delta);
        setStatusText(QStringLiteral("回复中"));
        return;
    }

    if (event.type == QStringLiteral("TEXT_MESSAGE_END")) {
        return;
    }

    if (event.type == QStringLiteral("RUN_FINISHED")) {
        finishCurrentReply();
        return;
    }

    if (event.type == QStringLiteral("RUN_ERROR")) {
        failCurrentReply(event.error.isEmpty() ? QStringLiteral("sidecar 返回错误") : event.error);
        return;
    }

    if (event.type == QStringLiteral("CUSTOM") && event.name == QString::fromUtf8(kExpressionEventName)) {
        requestPetExpression(
            event.value.value(QStringLiteral("state")).toString(),
            event.value.value(QStringLiteral("expression")).toString()
        );
    }
}

QVariantMap ChatController::messageObject(const QString &role, const QString &text, bool pending, bool error) const
{
    QVariantMap message;
    message.insert(QStringLiteral("role"), role);
    message.insert(QStringLiteral("text"), text);
    message.insert(QStringLiteral("pending"), pending);
    message.insert(QStringLiteral("error"), error);
    return message;
}

void ChatController::appendMessage(const QVariantMap &message)
{
    m_messages.append(message);
    emit messagesChanged();
}

void ChatController::appendAssistantDelta(const QString &delta)
{
    if (m_assistantMessageIndex < 0 || m_assistantMessageIndex >= m_messages.size()) {
        appendMessage(messageObject(QStringLiteral("assistant"), delta, true, false));
        m_assistantMessageIndex = m_messages.size() - 1;
        return;
    }

    QVariantMap message = m_messages[m_assistantMessageIndex].toMap();
    message.insert(QStringLiteral("text"), message.value(QStringLiteral("text")).toString() + delta);
    message.insert(QStringLiteral("pending"), true);
    m_messages[m_assistantMessageIndex] = message;
    emit messagesChanged();
}

void ChatController::setSidecarReady(bool ready)
{
    if (m_sidecarReady == ready) {
        return;
    }
    m_sidecarReady = ready;
    emit sidecarReadyChanged();
}

void ChatController::setSending(bool sending)
{
    if (m_sending == sending) {
        return;
    }
    m_sending = sending;
    emit sendingChanged();
}

void ChatController::setStatusText(const QString &statusText)
{
    if (m_statusText == statusText) {
        return;
    }
    m_statusText = statusText;
    emit statusTextChanged();
}

void ChatController::requestPetExpression(const QString &state, const QString &expression)
{
    const QString normalizedState = state.trimmed().isEmpty() ? QStringLiteral("idle") : state.trimmed();
    const QString normalizedExpression = expression.trimmed().isEmpty() ? QStringLiteral("neutral") : expression.trimmed();
    m_runtime->requestExpression(normalizedState, normalizedExpression);
}

QString ChatController::sidecarExecutablePath() const
{
    const QString envPath = QProcessEnvironment::systemEnvironment().value(QStringLiteral("MILESEDGEWORTH_AGENT_PATH")).trimmed();
    if (!envPath.isEmpty() && QFileInfo::exists(envPath)) {
        return envPath;
    }

    const QString candidate = QDir(QCoreApplication::applicationDirPath()).filePath(executableName());
    if (QFileInfo::exists(candidate)) {
        return candidate;
    }

    return {};
}

void ChatController::handleStreamBytes(const QByteArray &bytes)
{
    const QList<ChatStreamEvent> events = m_parser.ingest(bytes);
    for (const ChatStreamEvent &event : events) {
        applyStreamEvent(event);
    }
}

void ChatController::finishCurrentReply()
{
    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap message = m_messages[m_assistantMessageIndex].toMap();
        message.insert(QStringLiteral("pending"), false);
        m_messages[m_assistantMessageIndex] = message;
        emit messagesChanged();
    }

    m_assistantMessageIndex = -1;
    setSending(false);
    setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    requestPetExpression(QStringLiteral("idle"), QStringLiteral("neutral"));
}

void ChatController::failCurrentReply(const QString &message)
{
    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap failedMessage = m_messages[m_assistantMessageIndex].toMap();
        failedMessage.insert(QStringLiteral("text"), message);
        failedMessage.insert(QStringLiteral("pending"), false);
        failedMessage.insert(QStringLiteral("error"), true);
        m_messages[m_assistantMessageIndex] = failedMessage;
        emit messagesChanged();
    } else {
        appendMessage(messageObject(QStringLiteral("assistant"), message, false, true));
    }

    m_assistantMessageIndex = -1;
    setSending(false);
    setStatusText(QStringLiteral("错误"));
    requestPetExpression(QStringLiteral("error"), QStringLiteral("neutral"));
}
```

- [ ] **Step 4: Update Miles AI state mappings**

`apps/desktop/resources/skins/miles-edgeworth/manifest.json` 有三处分散修改。**用精确 search-and-replace，不要靠 "类似上下文" 猜位置**——manifest 同名字段在不同 section 重复出现，模糊匹配会错乱。

**4-1. expressions[neutral].allowedStates 加入 "error"**（在 `expressions` 数组里，找 `"id": "neutral"` 那一项）：

old_string:

```json
    {
      "id": "neutral",
      "label": "默认",
      "description": "没有更合适表达时使用的平稳站立表达",
      "allowedStates": ["idle", "thinking", "speaking"],
      "priority": 0
    },
```

new_string:

```json
    {
      "id": "neutral",
      "label": "默认",
      "description": "没有更合适表达时使用的平稳站立表达",
      "allowedStates": ["idle", "thinking", "speaking", "error"],
      "priority": 0
    },
```

**4-2. expressionMappings.neutral 替换整段**：

old_string:

```json
    "neutral": {
      "selection": "first_available",
      "actions": [
        { "action": "idle_stand", "allowedStates": ["idle", "thinking", "speaking"] }
      ]
    },
```

new_string:

```json
    "neutral": {
      "selection": "first_available",
      "actions": [
        { "action": "thinking", "allowedStates": ["thinking"] },
        { "action": "idle_stand", "allowedStates": ["idle", "speaking", "error"] }
      ]
    },
```

> contract test 会断言 `'"action": "thinking", "allowedStates": ["thinking"]'` 这段字符串原样出现，请保留双引号、空格和顺序。

**4-3. states 加 error**：

old_string:

```json
  "states": {
    "idle": {
      "action": "idle_stand"
    },
    "thinking": {
      "action": "thinking"
    },
    "speaking": {
      "action": "objecting"
    }
  },
```

new_string:

```json
  "states": {
    "idle": {
      "action": "idle_stand"
    },
    "thinking": {
      "action": "thinking"
    },
    "speaking": {
      "action": "objecting"
    },
    "error": {
      "action": "idle_stand"
    }
  },
```

This keeps `thinking` and `error` skin-driven while still using the Phase 1 `requestExpression(state, expression)` path. **Phase 2.0 范围内**：error 状态视觉上和 idle 一样（都是 `idle_stand`），失败反馈靠 ChatWindow 的红色气泡；后续 Phase 2.4 再设计专门的 error 动作。

**4-4. 验证 manifest 仍然是合法 JSON**：

```bash
python3 -c "import json; json.load(open('apps/desktop/resources/skins/miles-edgeworth/manifest.json'))"
```

期望：无输出（解析成功）。如果报错，回退三处修改重新做。

- [ ] **Step 5: Wire controller test in desktop CMake**

In `apps/desktop/CMakeLists.txt`, add `Network` to `find_package` and link dependencies:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Widgets Multimedia Network QuickControls2)
```

Add this controller smoke target under `if(BUILD_TESTING)`:

```cmake
    qt_add_executable(ChatControllerSmoke
        tests/chat_controller_smoke.cpp
        src/chat/ChatController.cpp
        src/chat/ChatController.h
        src/chat/ChatStreamEvent.cpp
        src/chat/ChatStreamEvent.h
        src/pet/behavior/BehaviorTriggerEngine.cpp
        src/pet/behavior/BehaviorTriggerEngine.h
        src/pet/commands/SkinCommand.h
        src/pet/commands/SkinCommandResolver.cpp
        src/pet/commands/SkinCommandResolver.h
        src/pet/effects/AudioController.cpp
        src/pet/effects/AudioController.h
        src/pet/effects/PropController.cpp
        src/pet/effects/PropController.h
        src/pet/effects/PropState.h
        src/pet/events/PetEvent.h
        src/pet/events/PetEventBridge.cpp
        src/pet/events/PetEventBridge.h
        src/pet/interaction/CustomInteractionRegistry.cpp
        src/pet/interaction/CustomInteractionRegistry.h
        src/pet/interaction/GestureTracker.cpp
        src/pet/interaction/GestureTracker.h
        src/pet/interaction/HitZoneMatcher.cpp
        src/pet/interaction/HitZoneMatcher.h
        src/pet/interaction/InteractionPipeline.cpp
        src/pet/interaction/InteractionPipeline.h
        src/pet/manifest/SkinManifest.h
        src/pet/manifest/SkinManifestLoader.cpp
        src/pet/manifest/SkinManifestLoader.h
        src/pet/PetRuntime.cpp
        src/pet/PetRuntime.h
        src/pet/PetRuntimeSkin.cpp
        src/pet/requests/ActionRequest.h
        src/pet/runtime/RuntimeSnapshot.h
        src/pet/selection/ActionPoolSelector.cpp
        src/pet/selection/ActionPoolSelector.h
        src/pet/selection/ExpressionMappingResolver.cpp
        src/pet/selection/ExpressionMappingResolver.h
        src/skins/miles-edgeworth/MilesEdgeworthInteractions.cpp
        src/skins/miles-edgeworth/MilesEdgeworthInteractions.h
        src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.cpp
        src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.h
        resources/pet_assets.qrc
    )

    target_include_directories(ChatControllerSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pet
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pet/events
    )

    target_link_libraries(ChatControllerSmoke
        PRIVATE
            Qt6::Core
            Qt6::Qml
            Qt6::Network
    )

    add_test(NAME chat_controller_smoke COMMAND ChatControllerSmoke)
```

- [ ] **Step 6: Build and run controller test**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target ChatControllerSmoke
ctest --test-dir build -R chat_controller_smoke --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

Run:

```bash
git add apps/desktop/src/chat apps/desktop/tests/chat_controller_smoke.cpp apps/desktop/CMakeLists.txt apps/desktop/resources/skins/miles-edgeworth/manifest.json
git commit -m "feat: add desktop chat controller"
```

Expected: commit succeeds.

## Task 5: Chat Window and Menu Entry

**Files:**
- Create: `apps/desktop/qml/ChatWindow.qml`
- Modify: `apps/desktop/src/main.cpp`
- Modify: `apps/desktop/src/pet/surface/PetContextMenu.h`
- Modify: `apps/desktop/src/pet/surface/PetContextMenu.cpp`
- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.h`
- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Add ChatWindow QML**

Create `apps/desktop/qml/ChatWindow.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MilesEdgeworth as App

ApplicationWindow {
    id: chatWindow

    width: 420
    height: 560
    minimumWidth: 360
    minimumHeight: 420
    visible: false
    title: "Miles Chat"
    color: "#f7f4ef"

    function open() {
        show()
        raise()
        requestActivate()
        input.forceActiveFocus()
    }

    Connections {
        target: App.ChatController
        function onOpenWindowRequested() {
            chatWindow.open()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        function submitInput() {
            if (App.ChatController.sending) {
                App.ChatController.cancelCurrentReply()
                return
            }

            const text = input.text.trim()
            if (text.length === 0) {
                return
            }
            input.text = ""
            App.ChatController.sendMessage(text)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Miles"
                color: "#26201b"
                font.pixelSize: 18
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            Label {
                text: App.ChatController.statusText
                color: App.ChatController.sidecarReady ? "#386641" : "#8a4b38"
                font.pixelSize: 12
            }

            Button {
                text: "重连"
                enabled: !App.ChatController.sending
                onClicked: {
                    App.ChatController.startSidecar()
                    App.ChatController.checkHealth()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#fffdf8"
            border.color: "#d8d1c8"

            ListView {
                id: transcript

                anchors.fill: parent
                anchors.margins: 10
                clip: true
                spacing: 8
                model: App.ChatController.messages

                delegate: Item {
                    required property var modelData

                    width: transcript.width
                    height: bubble.implicitHeight + 4

                    Rectangle {
                        id: bubble

                        readonly property bool isUser: modelData.role === "user"

                        width: Math.min(parent.width * 0.82, messageText.implicitWidth + 24)
                        implicitHeight: messageText.implicitHeight + 16
                        anchors.right: isUser ? parent.right : undefined
                        anchors.left: isUser ? undefined : parent.left
                        radius: 6
                        color: modelData.error ? "#f6d6cc" : (isUser ? "#dce7f7" : "#eee7da")
                        border.color: modelData.error ? "#b65a45" : "transparent"

                        Text {
                            id: messageText

                            anchors.fill: parent
                            anchors.margins: 8
                            text: modelData.text.length > 0 ? modelData.text : "…"
                            color: "#26201b"
                            font.pixelSize: 14
                            wrapMode: Text.Wrap
                        }
                    }
                }

                onCountChanged: Qt.callLater(function() {
                    transcript.positionViewAtEnd()
                })
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextArea {
                id: input

                Layout.fillWidth: true
                Layout.preferredHeight: 72
                wrapMode: TextArea.Wrap
                placeholderText: "输入消息"
                enabled: App.ChatController.sidecarReady && !App.ChatController.sending

                Keys.onPressed: function(event) {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && (event.modifiers & Qt.ShiftModifier) === 0) {
                        event.accepted = true
                        submitInput()
                    }
                }
            }

            Button {
                id: sendButton

                text: App.ChatController.sending ? "停止" : "发送"
                Layout.preferredWidth: 76
                Layout.preferredHeight: 72

                onClicked: submitInput()
            }
        }
    }
}
```

- [ ] **Step 2: Register QML and controller sources**

In `apps/desktop/CMakeLists.txt`, add to `DESKTOP_SOURCES`:

```cmake
    src/chat/ChatController.cpp
    src/chat/ChatController.h
    src/chat/ChatStreamEvent.cpp
    src/chat/ChatStreamEvent.h
```

In `qt_add_qml_module`, add:

```cmake
        qml/ChatWindow.qml
```

In `qt_add_qml_module(... SOURCES ...)`, add:

```cmake
        src/chat/ChatController.h
```

In `target_link_libraries(MilesEdgeworthDesktop ...)`, add:

```cmake
        Qt6::Network
        Qt6::QuickControls2
```

- [ ] **Step 3: Add menu/controller parameter declarations**

In `apps/desktop/src/pet/surface/PetContextMenu.h`, add:

```cpp
class ChatController;
```

Change `show(...)` to:

```cpp
    static void show(
        QWidget *parent,
        PetRuntime *runtime,
        PetEventBridge *eventBridge,
        DesktopShellController *shellController,
        ChatController *chatController,
        const QPoint &globalPosition
    );
```

In `apps/desktop/src/pet/surface/PetSurfaceWindow.h`, add a forward declaration:

```cpp
class ChatController;
```

Change the constructor to:

```cpp
    explicit PetSurfaceWindow(
        PetRuntime *runtime,
        PetEventBridge *eventBridge,
        DesktopShellController *shellController,
        ChatController *chatController,
        QWidget *parent = nullptr
    );
```

Add the member:

```cpp
    ChatController *m_chatController = nullptr;
```

- [ ] **Step 4: Wire menu action**

In `apps/desktop/src/pet/surface/PetContextMenu.cpp`, include:

```cpp
#include "chat/ChatController.h"
```

Change the function signature to include `ChatController *chatController`.

After the mute action, add:

```cpp
    QAction *chatAction = menu.addAction(QStringLiteral("聊天"));
    QObject::connect(chatAction, &QAction::triggered, parent, [chatController]() {
        if (chatController != nullptr) {
            chatController->openWindow();
        }
    });
```

In `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`, include:

```cpp
#include "chat/ChatController.h"
```

Change the constructor initializer list:

```cpp
    , m_shellController(shellController)
    , m_chatController(chatController)
```

> 不要写 `Q_ASSERT(m_chatController != nullptr)`：Release 构建里 Q_ASSERT 是 no-op，给不了真正的保护。`PetContextMenu::show` 已经做了 `if (chatController != nullptr)` 检查，nullptr 的话菜单"聊天"项只是无响应，不会 crash。
>
> 长期来看 PetSurfaceWindow 持有 `ChatController*` 是 Phase 2.0 内的临时耦合；Phase 2.4 / 后续 plan 会把"打开聊天"重构成通过 menu command + PetEventBridge 转发，让 PetSurfaceWindow 不再认识 ChatController。

Change the `PetContextMenu::show(...)` call in `showContextMenuAt` to pass `m_chatController`.

- [ ] **Step 5: Load ChatWindow in main**

In `apps/desktop/src/main.cpp`, add includes:

```cpp
#include "chat/ChatController.h"

#include <QQmlApplicationEngine>
```

After creating `PetEventBridge`, add:

```cpp
    ChatController chatController(&petRuntime);
    ChatControllerForeign::s_instance = &chatController;

    QQmlApplicationEngine chatEngine;
    chatEngine.loadFromModule("MilesEdgeworth", "ChatWindow");
    if (chatEngine.rootObjects().isEmpty()) {
        return 1;
    }
    chatController.startSidecar();
```

Change the pet window construction to:

```cpp
    PetSurfaceWindow petSurfaceWindow(&petRuntime, &petEventBridge, &shellController, &chatController);
```

- [ ] **Step 6: Build desktop target**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target MilesEdgeworthDesktop
```

Expected: PASS.

- [ ] **Step 7: Commit**

Run:

```bash
git add apps/desktop/qml/ChatWindow.qml apps/desktop/src/main.cpp apps/desktop/src/pet/surface/PetContextMenu.* apps/desktop/src/pet/surface/PetSurfaceWindow.* apps/desktop/CMakeLists.txt
git commit -m "feat: add chat window entry"
```

Expected: commit succeeds.

## Task 6: Build Wiring for Go Sidecar

**Files:**
- Create: `apps/agent-core/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Add agent-core CMake wrapper**

Create `apps/agent-core/CMakeLists.txt`:

```cmake
find_program(GO_EXECUTABLE go)

if(GO_EXECUTABLE)
    set(MILES_AGENT_BINARY "${CMAKE_CURRENT_BINARY_DIR}/miles-agent${CMAKE_EXECUTABLE_SUFFIX}")

    add_custom_command(
        OUTPUT "${MILES_AGENT_BINARY}"
        COMMAND "${GO_EXECUTABLE}" build -o "${MILES_AGENT_BINARY}" ./cmd/miles-agent
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DEPENDS
            go.mod
            cmd/miles-agent/main.go
            internal/api/server.go
            internal/chat/provider.go
            internal/chat/mock_provider.go
        VERBATIM
    )

    add_custom_target(MilesAgentCore ALL DEPENDS "${MILES_AGENT_BINARY}")
    set(MILES_AGENT_BINARY "${MILES_AGENT_BINARY}" CACHE FILEPATH "Miles agent sidecar binary" FORCE)
else()
    message(WARNING "Go executable not found; MilesAgentCore target will not be built")
endif()
```

- [ ] **Step 2: Add root CMake subdirectory**

In root `CMakeLists.txt`, change:

```cmake
add_subdirectory(apps/desktop)
```

to:

```cmake
add_subdirectory(apps/agent-core)
add_subdirectory(apps/desktop)
```

- [ ] **Step 3: Copy sidecar next to desktop executable**

In `apps/desktop/CMakeLists.txt`, after `qt_finalize_executable(MilesEdgeworthDesktop)`, add:

```cmake
if(TARGET MilesAgentCore)
    add_dependencies(MilesEdgeworthDesktop MilesAgentCore)
    add_custom_command(
        TARGET MilesEdgeworthDesktop
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MILES_AGENT_BINARY}"
                "$<TARGET_FILE_DIR:MilesEdgeworthDesktop>/miles-agent${CMAKE_EXECUTABLE_SUFFIX}"
        VERBATIM
    )
endif()
```

- [ ] **Step 4: Build desktop and sidecar through CMake**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target MilesEdgeworthDesktop
```

Expected:
- `MilesAgentCore` builds if `go` is available.
- `miles-agent` is copied next to the `MilesEdgeworthDesktop` executable.
- `MilesEdgeworthDesktop` builds.

- [ ] **Step 5: Run sidecar tests and desktop smoke tests**

Run（合并为一条复合命令，避免 agent shell 工具不维持工作目录的问题）：

```bash
(cd apps/agent-core && go test ./...) && \
    ctest --test-dir build -R "chat_stream_event_parser_smoke|chat_controller_smoke" --output-on-failure
```

Expected: all selected tests PASS.

- [ ] **Step 6: Commit**

Run:

```bash
git add CMakeLists.txt apps/agent-core/CMakeLists.txt apps/desktop/CMakeLists.txt
git commit -m "build: wire mock agent sidecar"
```

Expected: commit succeeds.

## Task 7: Register Tests and Document Phase 2.0

**Files:**
- Modify: `CMakeLists.txt`
- Create: `docs/v2/阶段记录/Phase 2.0 AI Chat MVP 骨架.md`
- Modify: `docs/v2/文档索引.md`

- [ ] **Step 1: Register Phase 2.0 static check**

In root `CMakeLists.txt`, inside the existing `if(BUILD_TESTING)` / `if(Python3_Interpreter_FOUND)` block after Phase 1.4 checks, add:

```cmake
        # Phase 2.0 的 AI Chat MVP 骨架检查：守住 Go sidecar、
        # Qt ChatController、QML ChatWindow、SSE envelope 和 expression 联动边界。
        add_test(
            NAME check_phase_2_0_ai_chat_mvp
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_2_0_ai_chat_mvp.py
        )
```

- [ ] **Step 2: Write Phase 2.0 stage record**

Create `docs/v2/阶段记录/Phase 2.0 AI Chat MVP 骨架.md`:

```markdown
# Phase 2.0 AI Chat MVP 骨架

本文记录 Phase 2.0 的落地范围、验收方式和刻意不做的内容。Phase 2.0 只打通 AI 聊天桌宠的最小闭环，不接真实 provider。

## 1. 范围

- 新增 `apps/agent-core` Go sidecar。
- sidecar 暴露 `GET /health` 和 `POST /v1/chat/messages`。
- `POST /v1/chat/messages` 使用 `text/event-stream` 返回 mock provider 事件。
- Qt 新增 `ChatStreamEventParser` 和 `ChatController`。
- Qt 新增 QML `ChatWindow`。
- 原生桌宠右键菜单新增 `聊天` 入口。
- ChatController 将 `miles.pet.expression.requested` 事件转成 `PetRuntime::requestExpression(state, expression)`。

## 2. 当前事件 envelope

Phase 2.0 使用以下事件类型：

```text
RUN_STARTED
TEXT_MESSAGE_START
TEXT_MESSAGE_CONTENT
TEXT_MESSAGE_END
RUN_FINISHED
RUN_ERROR
CUSTOM
```

桌宠扩展事件：

```json
{
  "type": "CUSTOM",
  "name": "miles.pet.expression.requested",
  "value": {
    "state": "speaking",
    "expression": "objection"
  }
}
```

## 3. 验收

- 无真实 API key 时，mock provider 可以流式返回文本。
- 发送消息后，桌宠先进入 `thinking`。
- 收到 speaking expression 后，桌宠通过当前 Miles manifest 映射到 `objecting`。
- 流结束后，桌宠回到 `idle`。
- `go test ./...` 覆盖 sidecar health 和 mock stream。
- `chat_stream_event_parser_smoke` 覆盖 SSE 分片解析。
- `chat_controller_smoke` 覆盖 stream event 到 `PetRuntime::requestExpression` 的桥接。
- `check_phase_2_0_ai_chat_mvp.py` 覆盖文件结构和边界契约。

## 4. 非目标

- 不调用真实 OpenAI-compatible provider。
- 不保存 API key。
- 不提供模型配置 UI。
- 不保存会话历史。
- 不实现 persona prompt。
- 不接 tools、skills、MCP、权限确认或插件。

## 5. 下一步

Phase 2.1 在这个骨架上增加 OpenAI-compatible provider interface、环境变量或本地开发配置读取、真实流式 Chat Completions 适配，以及 provider 错误状态展示。
```

- [ ] **Step 3: Update v2 document index**

In `docs/v2/文档索引.md`, add this bullet under `阶段记录`:

```markdown
- [Phase 2.0 AI Chat MVP 骨架](阶段记录/Phase%202.0%20AI%20Chat%20MVP%20骨架.md)：AI 聊天最小闭环的 Go sidecar、Qt ChatController、QML ChatWindow、mock stream 和 expression 联动验收记录。
```

- [ ] **Step 4: Run contract check**

Run:

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
```

Expected:

```text
phase 2.0 ai chat mvp contract ok
```

- [ ] **Step 5: Run registered CTest subset**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
ctest --test-dir build -R "check_phase_2_0_ai_chat_mvp|chat_stream_event_parser_smoke|chat_controller_smoke" --output-on-failure
```

Expected: all selected tests PASS.

- [ ] **Step 6: Commit**

Run:

```bash
git add CMakeLists.txt docs/v2/文档索引.md docs/v2/阶段记录/Phase\ 2.0\ AI\ Chat\ MVP\ 骨架.md
git commit -m "docs: record phase 2.0 chat skeleton"
```

Expected: commit succeeds.

## Task 8: End-to-End Verification

**Files:**
- No new files expected.

- [ ] **Step 1: Run Go tests**

Run:

```bash
cd apps/agent-core && go test ./...
```

Expected: PASS.

- [ ] **Step 2: Run full CMake build**

Run:

```bash
cd /Users/tian/projects/my-projects/MilesEdgeworth
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build
```

Expected: PASS.

- [ ] **Step 3: Run full CTest**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests PASS, including:

```text
chat_stream_event_parser_smoke
chat_controller_smoke
check_phase_2_0_ai_chat_mvp
```

- [ ] **Step 4: Run manual desktop smoke**

Run:

```bash
./build/apps/desktop/MilesEdgeworthDesktop.app/Contents/MacOS/MilesEdgeworthDesktop
```

Expected on macOS:
- Pet appears.
- Right-click pet and choose `聊天`.
- Chat window opens.
- Type `hello` and press Enter.
- Chat window shows streamed mock reply.
- During request, pet switches to thinking and then speaking.
- After stream ends, pet returns to idle.

If the binary path differs on a non-bundle platform, run the executable printed by:

```bash
find build/apps/desktop -maxdepth 3 -type f -name 'MilesEdgeworthDesktop*' -perm -111
```

- [ ] **Step 5: Inspect final diff**

Run:

```bash
git status --short
git diff --stat HEAD
```

Expected:
- Working tree contains only intentional uncommitted verification artifacts, or is clean if every task commit was made.
- Diff stat is limited to Phase 2.0 files listed in this plan.

## Self-Review Notes

Spec coverage:
- Go sidecar: Task 2 and Task 6.
- `/health`: Task 2.
- Mock stream without API key: Task 2 and Task 8.
- Event envelope: Task 2 and Task 3.
- ChatController bridge: Task 4.
- ChatWindow: Task 5.
- PetRuntime expression mapping: Task 4 and Task 8.
- CTest / Go test coverage: Task 2, Task 3, Task 4, Task 7, Task 8.
- Documentation record: Task 7.

Placeholder scan:
- The plan uses exact file paths, event names, code snippets, commands, and expected outputs.
- The plan intentionally excludes Phase 2.1+ provider/config/security work.

Type consistency:
- Go event type is `chat.StreamEvent`.
- C++ event type is `ChatStreamEvent`.
- C++ parser type is `ChatStreamEventParser`.
- QML singleton is `App.ChatController` via `ChatControllerForeign`.
- Pet expression event name is consistently `miles.pet.expression.requested`.

## Phase 2.0 后续可改进项（不阻塞）

下面这些在 Phase 2.0 review 时被识别为"知道但本期不做"，提示给 Phase 2.1+ 时一并考虑：

- **PetSurfaceWindow → ChatController 解耦**：当前 PetSurfaceWindow 直接持有 `ChatController *`，让桌宠表层模块编译期依赖 chat 模块。未来通过 `PetEventBridge::submitMenuCommand("chat.open")` + InteractionPipeline 路由解耦。Phase 2.4 整理 expression 体验时一起做。
- **ChatController 健康检查 retry**：当前启动后 200ms 单次 checkHealth，失败后需要用户手动点"重连"。macOS 首次启动二进制 codesign 期间容易踩到。Phase 2.1 引入真实 provider 时一起加指数退避。
- **ChatControllerSmoke 通过 PetRuntimeLib 简化**：当前 target 列了 30+ 个 source 文件。未来抽 PetRuntime + 依赖到 static library `PetRuntimeLib`，所有 smoke 都 link 它，新增 PetRuntime 依赖时不再手动维护。
- **error 状态视觉反馈**：Phase 2.0 内 error → idle_stand，仅 ChatWindow 红色气泡。Phase 2.4 加专门的 error 动作（皱眉 / 摇头 / 抱胸思考但带"困惑"标记）。
- **端口配置化**：当前 `127.0.0.1:39710` 写死。Phase 2.2 配置面板落地时改成"随机端口 + 通过环境变量传给 sidecar"，避免占用冲突。
- **sidecar 孤儿进程检测**：desktop 异常崩溃时 sidecar 可能成孤儿（占用端口）。Phase 2.2 评估"sidecar 监测父进程存活 + 自杀"或"启动时检测同端口已被占用且不是自己的子进程则释放再启动"。
