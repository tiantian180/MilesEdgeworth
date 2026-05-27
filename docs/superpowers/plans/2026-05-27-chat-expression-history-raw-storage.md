# Chat Expression History Raw Storage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve `[EXPR:x]` markers in assistant history sent back to the model while keeping all chat-window display text marker-free.

**Architecture:** Keep provider parsing unchanged: `StreamEvent.Delta` remains the clean UI text and `StreamEvent.RawDelta` remains the provider-origin content used for persistence and tracing. The API layer owns the boundary between model context and UI display: it stores assistant replies from raw stream content, passes stored raw assistant history to `ChatService`, and strips markers only when serving `/v1/conversations/{id}/messages` for UI restoration.

**Tech Stack:** Go 1.22, `net/http`, existing `internal/chat/expression` streaming parser, SQLite store package, Go `testing` and `httptest`.

---

## Scope Check

This plan implements one focused behavior change in the Go sidecar API layer.

Included:
- Persist completed assistant replies using `StreamEvent.RawDelta` when available.
- Persist partial assistant replies using the same raw-content path.
- Keep SSE `TEXT_MESSAGE_CONTENT.delta` unchanged so live chat display remains clean.
- Strip `[EXPR:x]` markers from assistant messages returned by `GET /v1/conversations/{id}/messages`.
- Keep `ChatService.BuildMessages()` unchanged so it reads raw assistant content directly from SQLite.
- Verify README does not need a user-facing update.

Excluded:
- No Qt / QML changes.
- No provider parser changes.
- No schema migration.
- No summary prompt behavior changes.
- No historical data migration for existing clean assistant rows.

## Design Context

Read these before editing:

- `docs/v2/设计方案/AI 聊天动画编排设计.md`
  - `TEXT_MESSAGE_CONTENT` is a UI display event.
  - `[EXPR:x]` markers do not enter `TEXT_MESSAGE_CONTENT`.
- `docs/v2/设计方案/会话历史与人设设计.md`
  - `messages.content` is model-context content, not necessarily UI text.
  - `assistant` rows store raw provider replies with `[EXPR:x]`.
  - `GET /v1/conversations/{id}/messages` returns clean assistant text for UI restoration.

## File Structure

Modify:

- `apps/agent-core/internal/api/server.go`
  - Owns chat HTTP/SSE handling and conversation message responses.
  - Add raw/display reply accumulation in `handleChatMessages`.
  - Add helper functions that choose persisted reply content and strip assistant display markers.

- `apps/agent-core/internal/api/server_test.go`
  - Add API tests for raw assistant persistence, clean SSE deltas, clean history restore, and raw partial persistence.
  - Existing fake provider can emit `RawDelta` directly; no new test helper file is needed.

No changes:

- `apps/agent-core/internal/chat/openai/provider.go`
  - It already emits `RawDelta` for raw model tokens.
- `apps/agent-core/internal/chat/service/service.go`
  - It already reads SQLite rows and passes `row.Content` to model messages.
- `apps/agent-core/internal/store/store.go`
  - Existing schema can store raw content without migration.
- `apps/desktop/**`
  - UI already consumes clean `Delta` live and `content` from message restore.

## Task 0: Preflight

**Files:**
- Read: `docs/v2/设计方案/AI 聊天动画编排设计.md`
- Read: `docs/v2/设计方案/会话历史与人设设计.md`
- Read: `apps/agent-core/internal/api/server.go`
- Read: `apps/agent-core/internal/api/server_test.go`

- [ ] **Step 1: Confirm worktree and scope**

Run:

```bash
git status --short --branch
```

Expected:

```text
## feature/chat-expression-history...origin/main
 M "docs/v2/设计方案/AI 聊天动画编排设计.md"
 M "docs/v2/设计方案/会话历史与人设设计.md"
```

If additional files are modified, inspect them with `git diff -- <path>` before continuing. Do not overwrite unrelated work.

- [ ] **Step 2: Run sidecar baseline tests**

Run:

```bash
go test ./...
```

Working directory:

```text
apps/agent-core
```

Expected: all packages pass.

- [ ] **Step 3: Confirm implementation target**

Run:

```bash
rg -n "RawDelta|var reply strings.Builder|messageResponses|persistPartialReply" apps/agent-core/internal/api apps/agent-core/internal/chat
```

Expected output includes:

```text
apps/agent-core/internal/chat/provider.go:40:	RawDelta  string         `json:"-"`
apps/agent-core/internal/api/server.go:210:	var reply strings.Builder
apps/agent-core/internal/api/server.go:297:func (s *Server) persistPartialReply
apps/agent-core/internal/api/server.go:353:func messageResponses
```

## Task 1: API Red Tests For Completed Replies And History Restore

**Files:**
- Modify: `apps/agent-core/internal/api/server_test.go`

- [ ] **Step 1: Add failing tests for completed assistant replies and restored history**

Insert these tests after `TestChatPersistsUserAndAssistant` and after `TestGetConversationMessagesIncludesPartialFlag`, respectively:

```go
func TestChatPersistsRawAssistantReplyButStreamsCleanText(t *testing.T) {
	provider := &fakeProvider{
		streamFunc: func(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
			_ = ctx
			events := make(chan chat.StreamEvent, 8)
			events <- chat.StreamEvent{Type: "RUN_STARTED", RunID: params.RunID}
			events <- chat.StreamEvent{
				Type:     "CUSTOM",
				Name:     "miles.pet.expression.requested",
				RunID:    params.RunID,
				RawDelta: "[EXPR:objection]",
				Value:    map[string]any{"state": "speaking", "expression": "objection"},
			}
			events <- chat.StreamEvent{
				Type:      "TEXT_MESSAGE_CONTENT",
				RunID:     params.RunID,
				MessageID: params.MessageID,
				Delta:     "异议あり。",
				RawDelta:  "异议あり。",
			}
			events <- chat.StreamEvent{Type: "RUN_FINISHED", RunID: params.RunID}
			close(events)
			return events, nil
		},
	}
	st, server := newTestServer(t, provider, 8192)
	conv := createConversation(t, st)

	resp := postChat(t, server.URL, fmt.Sprintf(`{"conversationId":%q,"message":"hello"}`, conv.ID))
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("chat status = %d, want 200", resp.StatusCode)
	}
	events := readSSEEvents(t, resp.Body)
	seenCleanDelta := false
	for _, event := range events {
		if strings.Contains(event.Delta, "[EXPR:") {
			t.Fatalf("SSE delta leaked expression marker: %+v", event)
		}
		if event.Type == "TEXT_MESSAGE_CONTENT" && event.Delta == "异议あり。" {
			seenCleanDelta = true
		}
	}
	if !seenCleanDelta {
		t.Fatalf("events = %+v, want clean text delta", events)
	}

	messages := getMessages(t, st, conv.ID)
	if len(messages) != 2 {
		t.Fatalf("message count = %d, want 2: %+v", len(messages), messages)
	}
	if messages[1].Role != store.RoleAssistant || messages[1].Content != "[EXPR:objection]异议あり。" || messages[1].IsPartial {
		t.Fatalf("stored assistant message = %+v, want raw expression reply", messages[1])
	}
}
```

```go
func TestGetConversationMessagesStripsExpressionTagsForAssistantContent(t *testing.T) {
	st, server := newTestServer(t, &fakeProvider{}, 8192)
	conv := createConversation(t, st)
	if _, err := st.AppendMessage(conv.ID, store.RoleUser, "検事", false); err != nil {
		t.Fatal(err)
	}
	if _, err := st.AppendMessage(conv.ID, store.RoleAssistant, "[EXPR:objection]异议あり。[EXPR:polite]失礼。", false); err != nil {
		t.Fatal(err)
	}

	resp, err := http.Get(server.URL + "/v1/conversations/" + conv.ID + "/messages")
	if err != nil {
		t.Fatalf("GET messages: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}

	var messages []store.Message
	if err := json.NewDecoder(resp.Body).Decode(&messages); err != nil {
		t.Fatalf("decode messages: %v", err)
	}
	if len(messages) != 2 {
		t.Fatalf("message count = %d, want 2", len(messages))
	}
	if messages[0].Content != "検事" {
		t.Fatalf("user content = %q, want unchanged text", messages[0].Content)
	}
	if messages[1].Content != "异议あり。失礼。" {
		t.Fatalf("assistant content = %q, want clean display text", messages[1].Content)
	}
}
```

- [ ] **Step 2: Format tests**

Run:

```bash
gofmt -w apps/agent-core/internal/api/server_test.go
```

- [ ] **Step 3: Run tests and verify they fail for the intended reasons**

Run:

```bash
go test ./internal/api -run 'TestChatPersistsRawAssistantReplyButStreamsCleanText|TestGetConversationMessagesStripsExpressionTagsForAssistantContent'
```

Working directory:

```text
apps/agent-core
```

Expected: both tests fail.

Expected failure details:

```text
Content:异议あり。
want raw expression reply
assistant content = "[EXPR:objection]异议あり。[EXPR:polite]失礼。", want clean display text
```

## Task 2: Implement Raw Persistence And Clean Message Restore

**Files:**
- Modify: `apps/agent-core/internal/api/server.go`
- Modify: `apps/agent-core/internal/api/server_test.go`

- [ ] **Step 1: Import the expression parser**

In `apps/agent-core/internal/api/server.go`, add the parser import beside the existing chat import:

```go
import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"net/http"
	"os"
	"strconv"
	"strings"

	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/expression"
	chatservice "milesedgeworth/agent-core/internal/chat/service"
	"milesedgeworth/agent-core/internal/store"
)
```

- [ ] **Step 2: Replace the single reply builder with display and raw builders**

In `handleChatMessages`, replace:

```go
	var reply strings.Builder
	partialPersisted := false
	persistPartialOnce := func(providerError bool) {
		if partialPersisted {
			return
		}
		if s.persistPartialReply(req.ConversationID, reply.String(), providerError) {
			partialPersisted = true
		}
	}
```

with:

```go
	var displayReply strings.Builder
	var rawReply strings.Builder
	partialPersisted := false
	persistPartialOnce := func(providerError bool) {
		if partialPersisted || providerError || displayReply.String() == "" {
			return
		}
		if s.persistPartialReply(req.ConversationID, replyContent(rawReply.String(), displayReply.String()), false) {
			partialPersisted = true
		}
	}
```

- [ ] **Step 3: Accumulate raw and display content from every stream event**

In the `for event := range events` loop, replace:

```go
		if event.Type == "TEXT_MESSAGE_CONTENT" {
			reply.WriteString(event.Delta)
		}
```

with:

```go
		if event.RawDelta != "" {
			rawReply.WriteString(event.RawDelta)
		}
		if event.Type == "TEXT_MESSAGE_CONTENT" {
			displayReply.WriteString(event.Delta)
			if event.RawDelta == "" {
				rawReply.WriteString(event.Delta)
			}
		}
```

This preserves provider-origin marker chunks from `CUSTOM` expression events and falls back to clean text for providers that do not populate `RawDelta`.

- [ ] **Step 4: Store raw content on normal completion**

Replace:

```go
	content := reply.String()
	if providerError || content == "" {
		return
	}
	if finished && ctx.Err() == nil {
		_, _ = s.store.AppendMessage(req.ConversationID, store.RoleAssistant, content, false)
		return
	}
	persistPartialOnce(false)
```

with:

```go
	content := displayReply.String()
	if providerError || content == "" {
		return
	}
	if finished && ctx.Err() == nil {
		_, _ = s.store.AppendMessage(req.ConversationID, store.RoleAssistant, replyContent(rawReply.String(), content), false)
		return
	}
	persistPartialOnce(false)
```

- [ ] **Step 5: Add helpers for persisted content and UI response content**

After `persistPartialReply`, add:

```go
func replyContent(rawContent, displayContent string) string {
	if rawContent != "" {
		return rawContent
	}
	return displayContent
}
```

Replace the `Content` assignment inside `messageResponses`:

```go
			Content:   msg.Content,
```

with:

```go
			Content:   responseContent(msg),
```

After `messageResponses`, add:

```go
func responseContent(msg store.Message) string {
	if msg.Role != store.RoleAssistant {
		return msg.Content
	}
	return stripExpressionMarkers(msg.Content)
}

func stripExpressionMarkers(content string) string {
	var out strings.Builder
	parser := expression.NewParser(nil, "neutral", func(text string) {
		out.WriteString(text)
	}, func(string) {})
	parser.Feed(content)
	parser.Flush()
	return out.String()
}
```

- [ ] **Step 6: Format implementation**

Run:

```bash
gofmt -w apps/agent-core/internal/api/server.go apps/agent-core/internal/api/server_test.go
```

- [ ] **Step 7: Run the targeted tests**

Run:

```bash
go test ./internal/api -run 'TestChatPersistsRawAssistantReplyButStreamsCleanText|TestGetConversationMessagesStripsExpressionTagsForAssistantContent'
```

Working directory:

```text
apps/agent-core
```

Expected:

```text
ok  	milesedgeworth/agent-core/internal/api
```

- [ ] **Step 8: Run the full API package**

Run:

```bash
go test ./internal/api
```

Expected:

```text
ok  	milesedgeworth/agent-core/internal/api
```

## Task 3: Cover Raw Partial Persistence

**Files:**
- Modify: `apps/agent-core/internal/api/server_test.go`

- [ ] **Step 1: Add a regression test for partial raw persistence**

Insert this test after `TestChatPersistsPartialOnceOnFlushError`:

```go
func TestChatPersistsRawPartialOnFlushError(t *testing.T) {
	provider := &fakeProvider{
		streamFunc: func(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
			_ = ctx
			events := make(chan chat.StreamEvent, 4)
			events <- chat.StreamEvent{Type: "RUN_STARTED", RunID: params.RunID}
			events <- chat.StreamEvent{
				Type:     "CUSTOM",
				Name:     "miles.pet.expression.requested",
				RunID:    params.RunID,
				RawDelta: "[EXPR:objection]",
				Value:    map[string]any{"state": "speaking", "expression": "objection"},
			}
			events <- chat.StreamEvent{
				Type:      "TEXT_MESSAGE_CONTENT",
				RunID:     params.RunID,
				MessageID: params.MessageID,
				Delta:     "途中",
				RawDelta:  "途中",
			}
			close(events)
			return events, nil
		},
	}
	st, handler := newTestHandler(t, provider, 8192)
	conv := createConversation(t, st)
	body := strings.NewReader(fmt.Sprintf(`{"conversationId":%q,"message":"hello"}`, conv.ID))
	req := httptest.NewRequest(http.MethodPost, "/v1/chat/messages", body)
	req.Header.Set("Content-Type", "application/json")
	rec := &flushErrorRecorder{
		header:      make(http.Header),
		failAtFlush: 3,
	}

	handler.ServeHTTP(rec, req)

	messages := getMessages(t, st, conv.ID)
	if len(messages) != 2 {
		t.Fatalf("message count = %d, want 2: %+v", len(messages), messages)
	}
	if messages[1].Role != store.RoleAssistant || messages[1].Content != "[EXPR:objection]途中" || !messages[1].IsPartial {
		t.Fatalf("partial message = %+v, want raw expression partial", messages[1])
	}
}
```

- [ ] **Step 2: Format tests**

Run:

```bash
gofmt -w apps/agent-core/internal/api/server_test.go
```

- [ ] **Step 3: Run the new partial test**

Run:

```bash
go test ./internal/api -run TestChatPersistsRawPartialOnFlushError
```

Expected:

```text
ok  	milesedgeworth/agent-core/internal/api
```

- [ ] **Step 4: Run all persistence-related API tests**

Run:

```bash
go test ./internal/api -run 'TestChatPersists|TestGetConversationMessages'
```

Expected:

```text
ok  	milesedgeworth/agent-core/internal/api
```

## Task 4: Verify ChatService Receives Raw Assistant History

**Files:**
- Modify: `apps/agent-core/internal/chat/service/service_test.go`

- [ ] **Step 1: Add a focused ChatService regression test**

Insert this test after `TestBuildMessagesPutsPersonaBeforeExpressionRules`:

```go
func TestBuildMessagesKeepsAssistantExpressionMarkers(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	appendMessage(t, s, conv.ID, store.RoleUser, "前の推論を覚えているか。")
	appendMessage(t, s, conv.ID, store.RoleAssistant, "[EXPR:objection]その推論には穴がある。")
	appendMessage(t, s, conv.ID, store.RoleUser, "続けて。")

	messages, _, err := newTestService(s, &fakeProvider{}, 8192).BuildMessages(context.Background(), BuildRequest{
		ConversationID: conv.ID,
	})
	if err != nil {
		t.Fatal(err)
	}

	var assistantContent string
	for _, msg := range messages {
		if msg.Role == store.RoleAssistant {
			assistantContent = msg.Content
			break
		}
	}
	if assistantContent != "[EXPR:objection]その推論には穴がある。" {
		t.Fatalf("assistant content = %q, want raw expression history", assistantContent)
	}
}
```

- [ ] **Step 2: Format test file**

Run:

```bash
gofmt -w apps/agent-core/internal/chat/service/service_test.go
```

- [ ] **Step 3: Run the ChatService test**

Run:

```bash
go test ./internal/chat/service -run TestBuildMessagesKeepsAssistantExpressionMarkers
```

Expected:

```text
ok  	milesedgeworth/agent-core/internal/chat/service
```

This should pass without production-code changes because `ChatService` already uses `row.Content`.

## Task 5: Full Verification And Documentation Check

**Files:**
- Read: `README.md`
- Read: `docs/v2/设计方案/AI 聊天动画编排设计.md`
- Read: `docs/v2/设计方案/会话历史与人设设计.md`
- Modify only if verification exposes a design mismatch.

- [ ] **Step 1: Run all Go tests**

Run:

```bash
go test ./...
```

Working directory:

```text
apps/agent-core
```

Expected: all packages pass.

- [ ] **Step 2: Run relevant static contract checks**

Run from repository root:

```bash
python3 tests/check_phase_2_1_provider.py
python3 tests/check_phase_2_3_2_session_persona.py
python3 tests/check_phase_2_4_phased_animation.py
```

Expected: all three scripts complete without `AssertionError`.

- [ ] **Step 3: Check formatting and whitespace**

Run:

```bash
git diff --check
```

Expected: no output.

- [ ] **Step 4: Check README impact**

Run:

```bash
rg -n "chat|history|EXPR|conversation" README.md
git diff -- README.md
```

Expected: `rg` may print existing README references, and `git diff -- README.md` prints no diff. No README update is needed because this change adjusts an internal persistence/display boundary rather than user-facing setup or usage.

- [ ] **Step 5: Review final diff**

Run:

```bash
git diff --stat
git diff -- apps/agent-core/internal/api/server.go apps/agent-core/internal/api/server_test.go apps/agent-core/internal/chat/service/service_test.go
git diff -- docs/v2/设计方案/AI\ 聊天动画编排设计.md docs/v2/设计方案/会话历史与人设设计.md
```

Expected:

```text
apps/agent-core/internal/api/server.go
apps/agent-core/internal/api/server_test.go
apps/agent-core/internal/chat/service/service_test.go
docs/v2/设计方案/AI 聊天动画编排设计.md
docs/v2/设计方案/会话历史与人设设计.md
```

The code diff must show:
- Raw assistant persistence uses `RawDelta` when present.
- SSE writing still serializes clean `Delta`; `RawDelta` is not exposed because it has `json:"-"`.
- `messageResponses` strips expression markers only for assistant messages.
- `ChatService` test confirms model context keeps assistant markers.

- [ ] **Step 6: Commit**

Run:

```bash
git add apps/agent-core/internal/api/server.go apps/agent-core/internal/api/server_test.go apps/agent-core/internal/chat/service/service_test.go docs/v2/设计方案/AI\ 聊天动画编排设计.md docs/v2/设计方案/会话历史与人设设计.md
git commit -m "fix: 保留助手历史中的表达标记"
```

Expected: commit succeeds on `feature/chat-expression-history`.

## Self-Review

Spec coverage:
- Model history includes `[EXPR:x]`: Task 2 stores raw assistant replies and Task 4 verifies `ChatService` keeps markers.
- Chat UI sees clean replies: Task 1 verifies SSE deltas are clean and restored history strips markers.
- Partial replies follow the same storage semantics: Task 3 verifies raw partial persistence.
- Design docs as long-term truth: Task 5 keeps the existing design-doc edits in the final diff and checks for mismatch.

Placeholder scan:
- The red-flag placeholder scan passed.
- Each code-changing step includes exact Go code to insert or replace.
- Each verification step includes exact commands and expected outcomes.

Type consistency:
- `chat.StreamEvent.RawDelta` matches `apps/agent-core/internal/chat/provider.go`.
- `store.RoleAssistant` matches existing store role constants used by API and service tests.
- `expression.NewParser(nil, "neutral", onText, onExpression)` matches the existing parser constructor.
- `messageResponse.Content` remains the API field consumed by Qt.
