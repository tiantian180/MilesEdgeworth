package api_test

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"milesedgeworth/agent-core/internal/api"
	"milesedgeworth/agent-core/internal/chat"
	chatservice "milesedgeworth/agent-core/internal/chat/service"
	"milesedgeworth/agent-core/internal/store"
)

type fakeCatalog struct {
	window int
}

func (c fakeCatalog) ContextWindow(string) int {
	return c.window
}

type fakeProvider struct {
	mu           sync.Mutex
	completeText string
	streamCalls  int
	streamParams chat.ChatParams
	streamFunc   func(context.Context, chat.ChatParams) (<-chan chat.StreamEvent, error)
}

func (p *fakeProvider) StreamChat(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
	p.mu.Lock()
	p.streamCalls++
	p.streamParams = params
	p.mu.Unlock()

	if p.streamFunc != nil {
		return p.streamFunc(ctx, params)
	}

	events := make(chan chat.StreamEvent, 1)
	events <- chat.StreamEvent{Type: "TEXT_MESSAGE_CONTENT", RunID: params.RunID, MessageID: params.MessageID, Delta: "异议あり。"}
	close(events)
	return events, nil
}

func (p *fakeProvider) Complete(context.Context, chat.ChatParams) (string, error) {
	if p.completeText != "" {
		return p.completeText, nil
	}
	return "压缩后的记忆", nil
}

func (p *fakeProvider) calls() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.streamCalls
}

func TestHealth(t *testing.T) {
	_, server := newTestServer(t, &fakeProvider{}, 8192)

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
	if body["provider"] != "test-provider" {
		t.Fatalf("provider = %v, want test-provider", body["provider"])
	}
}

func TestConversationCRUD(t *testing.T) {
	_, server := newTestServer(t, &fakeProvider{}, 8192)

	resp, err := http.Post(server.URL+"/v1/conversations", "application/json", strings.NewReader(`{"skinId":"miles-edgeworth"}`))
	if err != nil {
		t.Fatalf("POST /v1/conversations: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusCreated {
		t.Fatalf("create status = %d, want 201", resp.StatusCode)
	}

	var created store.Conversation
	if err := json.NewDecoder(resp.Body).Decode(&created); err != nil {
		t.Fatalf("decode created conversation: %v", err)
	}
	if created.ID == "" || created.SkinID != "miles-edgeworth" {
		t.Fatalf("created conversation = %+v", created)
	}

	resp, err = http.Get(server.URL + "/v1/conversations?limit=10")
	if err != nil {
		t.Fatalf("GET /v1/conversations: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("list status = %d, want 200", resp.StatusCode)
	}
	var list []store.Conversation
	if err := json.NewDecoder(resp.Body).Decode(&list); err != nil {
		t.Fatalf("decode conversation list: %v", err)
	}
	if len(list) != 1 || list[0].ID != created.ID || list[0].SkinID != "miles-edgeworth" {
		t.Fatalf("list = %+v, want created conversation", list)
	}

	req, err := http.NewRequest(http.MethodDelete, server.URL+"/v1/conversations/"+created.ID, nil)
	if err != nil {
		t.Fatal(err)
	}
	resp, err = http.DefaultClient.Do(req)
	if err != nil {
		t.Fatalf("DELETE /v1/conversations/{id}: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusNoContent {
		t.Fatalf("delete status = %d, want 204", resp.StatusCode)
	}

	resp, err = http.Get(server.URL + "/v1/conversations/" + created.ID + "/messages")
	if err != nil {
		t.Fatalf("GET deleted messages: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusNotFound {
		t.Fatalf("deleted messages status = %d, want 404", resp.StatusCode)
	}
}

func TestChatPersistsUserAndAssistant(t *testing.T) {
	st, server := newTestServer(t, &fakeProvider{}, 8192)
	conv := createConversation(t, st)

	resp := postChat(t, server.URL, fmt.Sprintf(`{"conversationId":%q,"message":"  待った  "}`, conv.ID))
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("chat status = %d, want 200", resp.StatusCode)
	}
	events := readSSEEvents(t, resp.Body)
	if len(events) == 0 {
		t.Fatal("got no SSE events")
	}

	messages := getMessages(t, st, conv.ID)
	if len(messages) != 2 {
		t.Fatalf("message count = %d, want 2: %+v", len(messages), messages)
	}
	if messages[0].Role != store.RoleUser || messages[0].Content != "待った" {
		t.Fatalf("user message = %+v", messages[0])
	}
	if messages[1].Role != store.RoleAssistant || messages[1].Content != "异议あり。" || messages[1].IsPartial {
		t.Fatalf("reply message = %+v", messages[1])
	}
}

func TestChatRejectsUnknownConversation(t *testing.T) {
	provider := &fakeProvider{}
	_, server := newTestServer(t, provider, 8192)

	resp := postChat(t, server.URL, `{"conversationId":"missing","message":"hello"}`)
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusNotFound {
		t.Fatalf("status = %d, want 404", resp.StatusCode)
	}
	if provider.calls() != 0 {
		t.Fatalf("provider calls = %d, want 0", provider.calls())
	}
}

func TestToolResultEndpointRoutesResultToWaitingRun(t *testing.T) {
	provider := &fakeProvider{}
	provider.streamFunc = func(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
		_ = ctx
		events := make(chan chat.StreamEvent, 3)
		switch provider.calls() {
		case 1:
			events <- chat.StreamEvent{
				Type:       "TOOL_CALL",
				RunID:      params.RunID,
				ToolCallID: "tc-1",
				ToolName:   "pet_motion",
				ToolArgs:   `{"action":"moveTo","x":0.5,"y":0.5}`,
			}
		default:
			events <- chat.StreamEvent{Type: "TEXT_MESSAGE_CONTENT", RunID: params.RunID, MessageID: params.MessageID, Delta: "到着しました。"}
		}
		close(events)
		return events, nil
	}
	st, server := newTestServer(t, provider, 8192)
	conv := createConversation(t, st)

	resp := postChat(t, server.URL, fmt.Sprintf(`{"conversationId":%q,"message":"move"}`, conv.ID))
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("chat status = %d, want 200", resp.StatusCode)
	}

	reader := bufio.NewReader(resp.Body)
	var toolEvent chat.StreamEvent
	for {
		event := readNextSSEEvent(t, reader)
		if event.Type == "TOOL_CALL" {
			toolEvent = event
			break
		}
	}

	wrongBody := fmt.Sprintf(`{"runId":%q,"toolCallId":"wrong","result":{"success":true}}`, toolEvent.RunID)
	wrongResp, err := http.Post(server.URL+"/v1/chat/tool-result", "application/json", strings.NewReader(wrongBody))
	if err != nil {
		t.Fatalf("POST mismatched /v1/chat/tool-result: %v", err)
	}
	defer wrongResp.Body.Close()
	if wrongResp.StatusCode != http.StatusNotFound {
		payload, _ := io.ReadAll(wrongResp.Body)
		t.Fatalf("mismatched tool result status = %d, want 404; body=%s", wrongResp.StatusCode, payload)
	}

	body := fmt.Sprintf(`{"runId":%q,"toolCallId":%q,"result":{"success":true}}`, toolEvent.RunID, toolEvent.ToolCallID)
	toolResp, err := http.Post(server.URL+"/v1/chat/tool-result", "application/json", strings.NewReader(body))
	if err != nil {
		t.Fatalf("POST /v1/chat/tool-result: %v", err)
	}
	defer toolResp.Body.Close()
	if toolResp.StatusCode != http.StatusAccepted {
		payload, _ := io.ReadAll(toolResp.Body)
		t.Fatalf("tool result status = %d, want 202; body=%s", toolResp.StatusCode, payload)
	}

	for {
		event := readNextSSEEvent(t, reader)
		if event.Type == "RUN_FINISHED" {
			break
		}
	}
	if provider.calls() != 2 {
		t.Fatalf("provider calls = %d, want 2", provider.calls())
	}
}

func TestToolResultEndpointReturns404ForStaleRun(t *testing.T) {
	_, handler := newTestHandler(t, nil, 8192)

	body := strings.NewReader(`{"runId":"missing-run","toolCallId":"tc-1","result":{"success":true}}`)
	req := httptest.NewRequest(http.MethodPost, "/v1/chat/tool-result", body)
	res := httptest.NewRecorder()

	handler.ServeHTTP(res, req)

	if res.Code != http.StatusNotFound {
		t.Fatalf("status = %d, want 404; body=%s", res.Code, res.Body.String())
	}
}

func TestToolResultEndpointRejectsBadRequests(t *testing.T) {
	_, handler := newTestHandler(t, nil, 8192)

	tests := []struct {
		name   string
		method string
		body   string
		want   int
	}{
		{name: "bad json", method: http.MethodPost, body: `{`, want: http.StatusBadRequest},
		{name: "missing run", method: http.MethodPost, body: `{"toolCallId":"tc-1","result":{"success":true}}`, want: http.StatusBadRequest},
		{name: "missing tool call", method: http.MethodPost, body: `{"runId":"run-1","result":{"success":true}}`, want: http.StatusBadRequest},
		{name: "missing result", method: http.MethodPost, body: `{"runId":"run-1","toolCallId":"tc-1"}`, want: http.StatusBadRequest},
		{name: "wrong method", method: http.MethodGet, body: ``, want: http.StatusMethodNotAllowed},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			req := httptest.NewRequest(tt.method, "/v1/chat/tool-result", strings.NewReader(tt.body))
			res := httptest.NewRecorder()

			handler.ServeHTTP(res, req)

			if res.Code != tt.want {
				t.Fatalf("status = %d, want %d; body=%s", res.Code, tt.want, res.Body.String())
			}
		})
	}
}

func TestGetConversationMessagesIncludesPartialFlag(t *testing.T) {
	st, server := newTestServer(t, &fakeProvider{}, 8192)
	conv := createConversation(t, st)
	if _, err := st.AppendMessage(conv.ID, store.RoleUser, "検事", false); err != nil {
		t.Fatal(err)
	}
	if _, err := st.AppendMessage(conv.ID, store.RoleAssistant, "途中まで", true); err != nil {
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
	if messages[0].IsPartial {
		t.Fatalf("user isPartial = true, want false")
	}
	if !messages[1].IsPartial {
		t.Fatalf("reply isPartial = false, want true")
	}
}

func TestChatPersistsPartialOnClientCancel(t *testing.T) {
	partialSent := make(chan struct{})
	provider := &fakeProvider{
		streamFunc: func(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
			events := make(chan chat.StreamEvent)
			go func() {
				defer close(events)
				events <- chat.StreamEvent{Type: "TEXT_MESSAGE_CONTENT", RunID: params.RunID, MessageID: params.MessageID, Delta: "途中"}
				close(partialSent)
				<-ctx.Done()
			}()
			return events, nil
		},
	}
	st, server := newTestServer(t, provider, 8192)
	conv := createConversation(t, st)

	ctx, cancel := context.WithCancel(context.Background())
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, server.URL+"/v1/chat/messages", strings.NewReader(fmt.Sprintf(`{"conversationId":%q,"message":"hello"}`, conv.ID)))
	if err != nil {
		t.Fatal(err)
	}
	req.Header.Set("Content-Type", "application/json")
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatalf("POST /v1/chat/messages: %v", err)
	}

	reader := bufio.NewReader(resp.Body)
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			t.Fatalf("read stream before cancel: %v", err)
		}
		if strings.Contains(line, `"delta":"途中"`) {
			break
		}
	}
	<-partialSent
	cancel()
	_ = resp.Body.Close()

	eventually(t, time.Second, func() bool {
		messages := getMessages(t, st, conv.ID)
		return len(messages) == 2 &&
			messages[1].Role == store.RoleAssistant &&
			messages[1].Content == "途中" &&
			messages[1].IsPartial
	})
}

func TestChatPersistsPartialOnceOnFlushError(t *testing.T) {
	provider := &fakeProvider{
		streamFunc: func(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
			_ = ctx
			events := make(chan chat.StreamEvent, 2)
			events <- chat.StreamEvent{Type: "TEXT_MESSAGE_CONTENT", RunID: params.RunID, MessageID: params.MessageID, Delta: "途中"}
			events <- chat.StreamEvent{Type: "TEXT_MESSAGE_CONTENT", RunID: params.RunID, MessageID: params.MessageID, Delta: "まで"}
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
	if messages[1].Role != store.RoleAssistant || messages[1].Content != "途中" || !messages[1].IsPartial {
		t.Fatalf("partial message = %+v", messages[1])
	}
}

func TestChatDoesNotPersistAssistantAfterRunError(t *testing.T) {
	provider := &fakeProvider{
		streamFunc: func(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
			_ = ctx
			events := make(chan chat.StreamEvent, 2)
			events <- chat.StreamEvent{Type: "TEXT_MESSAGE_CONTENT", RunID: params.RunID, MessageID: params.MessageID, Delta: "途中"}
			events <- chat.StreamEvent{Type: "RUN_ERROR", RunID: params.RunID, Error: "provider failed"}
			close(events)
			return events, nil
		},
	}
	st, server := newTestServer(t, provider, 8192)
	conv := createConversation(t, st)

	resp := postChat(t, server.URL, fmt.Sprintf(`{"conversationId":%q,"message":"hello"}`, conv.ID))
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}
	events := readSSEEvents(t, resp.Body)
	if got := events[len(events)-1].Type; got != "RUN_ERROR" {
		t.Fatalf("last event = %q, want RUN_ERROR", got)
	}

	messages := getMessages(t, st, conv.ID)
	if len(messages) != 1 {
		t.Fatalf("message count = %d, want only user row: %+v", len(messages), messages)
	}
	if messages[0].Role != store.RoleUser || messages[0].Content != "hello" {
		t.Fatalf("user message = %+v", messages[0])
	}
}

func TestChatWithoutProviderStreamsRunError(t *testing.T) {
	st, server := newTestServer(t, nil, 8192)
	conv := createConversation(t, st)

	resp := postChat(t, server.URL, fmt.Sprintf(`{"conversationId":%q,"message":"hello"}`, conv.ID))
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}
	events := readSSEEvents(t, resp.Body)
	if len(events) != 1 || events[0].Type != "RUN_ERROR" {
		t.Fatalf("events = %+v, want single RUN_ERROR", events)
	}
	if !strings.Contains(events[0].Error, "未配置模型") {
		t.Fatalf("RUN_ERROR = %q, want missing model config message", events[0].Error)
	}

	messages := getMessages(t, st, conv.ID)
	if len(messages) != 1 || messages[0].Role != store.RoleUser {
		t.Fatalf("messages = %+v, want only user row", messages)
	}
}

func TestSummarizingEventIsFirstSSEEventWhenTriggered(t *testing.T) {
	provider := &fakeProvider{completeText: "古い会話の要約"}
	st, server := newTestServer(t, provider, 2000)
	conv := createConversation(t, st)
	appendStoreMessage(t, st, conv.ID, store.RoleUser, strings.Repeat("古い証言", 120))
	appendStoreMessage(t, st, conv.ID, store.RoleAssistant, "記録した。")
	appendStoreMessage(t, st, conv.ID, store.RoleUser, "最近の質問")
	appendStoreMessage(t, st, conv.ID, store.RoleAssistant, "最近の回答")

	resp := postChat(t, server.URL, fmt.Sprintf(`{"conversationId":%q,"message":"今の質問"}`, conv.ID))
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}
	events := readSSEEvents(t, resp.Body)
	if len(events) == 0 {
		t.Fatal("got no SSE events")
	}
	if events[0].Type != "CUSTOM" || events[0].Name != chatservice.SummarizingEventName {
		t.Fatalf("first event = %+v, want summarizing event", events[0])
	}
}

func TestChatStreamRejectsEmptyMessage(t *testing.T) {
	_, server := newTestServer(t, &fakeProvider{}, 8192)

	resp := postChat(t, server.URL, `{"conversationId":"anything","message":"   "}`)
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusBadRequest {
		t.Fatalf("status = %d, want 400", resp.StatusCode)
	}
}

func TestChatStreamForwardsExpressions(t *testing.T) {
	provider := &fakeProvider{}
	st, server := newTestServer(t, provider, 8192)
	conv := createConversation(t, st)

	resp := postChat(t, server.URL, fmt.Sprintf(`{
		"conversationId": %q,
		"message": "hi",
		"expressions": [
			{"id": "objection", "description": "strong rebuttal"},
			{"id": "polite"}
		]
	}`, conv.ID))
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}
	_, _ = io.Copy(io.Discard, resp.Body)

	provider.mu.Lock()
	defer provider.mu.Unlock()
	got := strings.Join(provider.streamParams.KnownExpressionIDs, ",")
	if got != "objection,polite" {
		t.Fatalf("known expression ids = %q, want objection,polite", got)
	}
}

func TestChatStreamRejectsOversizedBody(t *testing.T) {
	_, server := newTestServer(t, &fakeProvider{}, 8192)

	huge := strings.Repeat("x", 2*1024*1024)
	body := fmt.Sprintf(`{"message":"%s"}`, huge)
	resp := postChat(t, server.URL, body)
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusRequestEntityTooLarge && resp.StatusCode != http.StatusBadRequest {
		t.Fatalf("status = %d, want 413 or 400", resp.StatusCode)
	}
}

func TestMethodNotAllowedReturnsJSONAndAllowHeader(t *testing.T) {
	_, server := newTestServer(t, &fakeProvider{}, 8192)

	req, err := http.NewRequest(http.MethodDelete, server.URL+"/v1/chat/messages", nil)
	if err != nil {
		t.Fatal(err)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatalf("DELETE /v1/chat/messages: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusMethodNotAllowed {
		t.Fatalf("status = %d, want 405", resp.StatusCode)
	}
	if got := resp.Header.Get("Allow"); got != http.MethodPost {
		t.Fatalf("Allow = %q, want POST", got)
	}
	if got := resp.Header.Get("Content-Type"); !strings.Contains(got, "application/json") {
		t.Fatalf("Content-Type = %q, want JSON", got)
	}
	var body map[string]string
	if err := json.NewDecoder(resp.Body).Decode(&body); err != nil {
		t.Fatalf("decode error body: %v", err)
	}
	if body["error"] != "method not allowed" {
		t.Fatalf("error body = %+v", body)
	}
}

func newTestServer(t *testing.T, provider chat.Provider, window int) (*store.Store, *httptest.Server) {
	t.Helper()
	st, handler := newTestHandler(t, provider, window)
	server := httptest.NewServer(handler)
	t.Cleanup(server.Close)
	return st, server
}

func newTestHandler(t *testing.T, provider chat.Provider, window int) (*store.Store, http.Handler) {
	t.Helper()
	st, err := store.Open(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = st.Close() })

	svc := chatservice.New(st, provider, fakeCatalog{window: window}, "test-model")
	return st, api.NewServer(st, svc, "test-provider").Routes()
}

func createConversation(t *testing.T, st *store.Store) store.Conversation {
	t.Helper()
	conv, err := st.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	return conv
}

func appendStoreMessage(t *testing.T, st *store.Store, conversationID, role, content string) {
	t.Helper()
	if _, err := st.AppendMessage(conversationID, role, content, false); err != nil {
		t.Fatal(err)
	}
}

func getMessages(t *testing.T, st *store.Store, conversationID string) []store.Message {
	t.Helper()
	messages, err := st.GetMessages(conversationID)
	if err != nil {
		t.Fatal(err)
	}
	return messages
}

func postChat(t *testing.T, baseURL, body string) *http.Response {
	t.Helper()
	resp, err := http.Post(baseURL+"/v1/chat/messages", "application/json", strings.NewReader(body))
	if err != nil {
		t.Fatalf("POST /v1/chat/messages: %v", err)
	}
	return resp
}

func readSSEEvents(t *testing.T, body io.Reader) []chat.StreamEvent {
	t.Helper()
	bodyBytes, err := io.ReadAll(body)
	if err != nil {
		t.Fatalf("read stream: %v", err)
	}
	var events []chat.StreamEvent
	for _, line := range bytes.Split(bodyBytes, []byte("\n")) {
		line = bytes.TrimSpace(line)
		if !bytes.HasPrefix(line, []byte("data: ")) {
			continue
		}
		var event chat.StreamEvent
		if err := json.Unmarshal(bytes.TrimPrefix(line, []byte("data: ")), &event); err != nil {
			t.Fatalf("decode SSE event %q: %v", line, err)
		}
		events = append(events, event)
	}
	return events
}

func readNextSSEEvent(t *testing.T, reader *bufio.Reader) chat.StreamEvent {
	t.Helper()

	type result struct {
		event chat.StreamEvent
		err   error
	}
	done := make(chan result, 1)
	go func() {
		for {
			line, err := reader.ReadBytes('\n')
			if err != nil {
				done <- result{err: err}
				return
			}
			line = bytes.TrimSpace(line)
			if !bytes.HasPrefix(line, []byte("data: ")) {
				continue
			}
			var event chat.StreamEvent
			if err := json.Unmarshal(bytes.TrimPrefix(line, []byte("data: ")), &event); err != nil {
				done <- result{err: fmt.Errorf("decode SSE event %q: %w", line, err)}
				return
			}
			done <- result{event: event}
			return
		}
	}()

	select {
	case got := <-done:
		if got.err != nil {
			t.Fatalf("read SSE event: %v", got.err)
		}
		return got.event
	case <-time.After(2 * time.Second):
		t.Fatal("timed out waiting for SSE event")
		return chat.StreamEvent{}
	}
}

func eventually(t *testing.T, timeout time.Duration, check func() bool) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if check() {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
	if !check() {
		t.Fatalf("condition not met within %s", timeout)
	}
}

type flushErrorRecorder struct {
	header      http.Header
	body        bytes.Buffer
	status      int
	flushes     int
	failAtFlush int
}

func (r *flushErrorRecorder) Header() http.Header {
	return r.header
}

func (r *flushErrorRecorder) WriteHeader(status int) {
	r.status = status
}

func (r *flushErrorRecorder) Write(p []byte) (int, error) {
	return r.body.Write(p)
}

func (r *flushErrorRecorder) FlushError() error {
	r.flushes++
	if r.flushes == r.failAtFlush {
		return errors.New("flush failed")
	}
	return nil
}
