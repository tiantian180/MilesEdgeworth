package openai_test

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/openai"
)

func float64Ptr(v float64) *float64 { return &v }

func intPtr(v int) *int { return &v }

func TestStreamChatHappyPath(t *testing.T) {
	// Upstream OpenAI-style SSE: deltas containing [EXPR:objection]...
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/v1/chat/completions" {
			t.Fatalf("upstream got %s", r.URL.Path)
		}
		var body struct {
			Model    string `json:"model"`
			Messages []struct {
				Role    string `json:"role"`
				Content string `json:"content"`
			} `json:"messages"`
			Stream bool `json:"stream"`
		}
		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			t.Fatalf("upstream decode: %v", err)
		}
		if !body.Stream || body.Model != "test-model" {
			t.Fatalf("unexpected upstream body: %+v", body)
		}
		gotMessages, err := json.Marshal(body.Messages)
		if err != nil {
			t.Fatalf("marshal messages: %v", err)
		}
		if got, want := string(gotMessages), `[{"role":"system","content":"persona\n\nexpr rules"},{"role":"user","content":"你好"}]`; got != want {
			t.Fatalf("messages = %s, want %s", got, want)
		}

		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		flusher := w.(http.Flusher)

		chunks := []string{
			`{"choices":[{"delta":{"content":"[EXPR:objection]"}}]}`,
			`{"choices":[{"delta":{"content":"异议！"}}]}`,
			`{"choices":[{"delta":{"content":"[EXPR:polite]"}}]}`,
			`{"choices":[{"delta":{"content":"再见。"}}]}`,
			`[DONE]`,
		}
		for _, c := range chunks {
			fmt.Fprintf(w, "data: %s\n\n", c)
			flusher.Flush()
		}
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "test-model", float64Ptr(0.5), intPtr(256))
	events, err := p.StreamChat(context.Background(), chat.ChatParams{
		RunID:     "test-run",
		MessageID: "test-message",
		Messages: []chat.Message{
			{Role: "system", Content: "persona\n\nexpr rules"},
			{Role: "user", Content: "你好"},
		},
		KnownExpressionIDs: []string{"neutral", "objection", "polite"},
	})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}

	var got []chat.StreamEvent
	for e := range events {
		got = append(got, e)
	}

	mustFind := func(predicate func(chat.StreamEvent) bool, what string) {
		t.Helper()
		for _, e := range got {
			if predicate(e) {
				return
			}
		}
		t.Fatalf("missing event: %s\nall: %+v", what, got)
	}

	mustFind(func(e chat.StreamEvent) bool { return e.Type == "RUN_STARTED" }, "RUN_STARTED")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "CUSTOM" && e.Name == "miles.pet.expression.requested" &&
			e.Value["state"] == "thinking"
	}, "thinking expression")
	mustFind(func(e chat.StreamEvent) bool { return e.Type == "TEXT_MESSAGE_START" }, "TEXT_MESSAGE_START")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "CUSTOM" && e.Value["state"] == "speaking" && e.Value["expression"] == "objection"
	}, "speaking objection expression")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "TEXT_MESSAGE_CONTENT" && e.Delta == "异议！"
	}, "objection text")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "CUSTOM" && e.Value["state"] == "speaking" && e.Value["expression"] == "polite"
	}, "speaking polite expression")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "TEXT_MESSAGE_CONTENT" && e.Delta == "再见。"
	}, "polite text")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "CUSTOM" && e.Value["state"] == "idle" &&
			e.Value["interruptHint"] == "afterCurrent"
	}, "idle afterCurrent expression")
	mustFind(func(e chat.StreamEvent) bool { return e.Type == "RUN_FINISHED" }, "RUN_FINISHED")
}

func TestStreamChatOmitsNilOptionalParams(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var body map[string]any
		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			t.Fatalf("upstream decode: %v", err)
		}
		if _, ok := body["temperature"]; ok {
			t.Fatalf("temperature should be omitted when nil: %+v", body)
		}
		if _, ok := body["max_tokens"]; ok {
			t.Fatalf("max_tokens should be omitted when nil: %+v", body)
		}
		w.Header().Set("Content-Type", "text/event-stream")
		fmt.Fprint(w, "data: [DONE]\n\n")
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "test-model", nil, nil)
	events, err := p.StreamChat(context.Background(), chat.ChatParams{
		Messages: []chat.Message{{Role: "user", Content: "hi"}},
	})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}
	for range events {
	}
}

func TestStreamChatAcceptsVersionedBaseURL(t *testing.T) {
	seenPath := ""
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		seenPath = r.URL.Path
		if seenPath != "/v1/chat/completions" {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "text/event-stream")
		fmt.Fprint(w, "data: [DONE]\n\n")
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL+"/v1", "sk-test", "test-model", float64Ptr(0.5), intPtr(256))
	events, err := p.StreamChat(context.Background(), chat.ChatParams{Messages: []chat.Message{{Role: "user", Content: "hi"}}})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}
	for range events {
	}
	if seenPath != "/v1/chat/completions" {
		t.Fatalf("requested path = %q, want /v1/chat/completions", seenPath)
	}
}

func TestStreamChatAcceptsProviderVersionPrefix(t *testing.T) {
	seenPath := ""
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		seenPath = r.URL.Path
		if seenPath != "/api/v3/chat/completions" {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "text/event-stream")
		fmt.Fprint(w, "data: [DONE]\n\n")
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL+"/api/v3", "sk-test", "test-model", float64Ptr(0.5), intPtr(256))
	events, err := p.StreamChat(context.Background(), chat.ChatParams{Messages: []chat.Message{{Role: "user", Content: "hi"}}})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}
	for range events {
	}
	if seenPath != "/api/v3/chat/completions" {
		t.Fatalf("requested path = %q, want /api/v3/chat/completions", seenPath)
	}
}

func TestStreamChatAcceptsFullChatCompletionsURL(t *testing.T) {
	seenPath := ""
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		seenPath = r.URL.Path
		if seenPath != "/custom/v1/chat/completions" {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "text/event-stream")
		fmt.Fprint(w, "data: [DONE]\n\n")
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL+"/custom/v1/chat/completions", "sk-test", "test-model", float64Ptr(0.5), intPtr(256))
	events, err := p.StreamChat(context.Background(), chat.ChatParams{Messages: []chat.Message{{Role: "user", Content: "hi"}}})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}
	for range events {
	}
	if seenPath != "/custom/v1/chat/completions" {
		t.Fatalf("requested path = %q, want /custom/v1/chat/completions", seenPath)
	}
}

func TestStreamChat4xxBecomesRunError(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/custom/v1/chat/completions" {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("X-Error-Code", "InvalidEndpointOrModel.NotFound")
		w.Header().Set("X-Request-Id", "req-123")
		http.Error(w, `{"error":{"message":"model not found"}}`, http.StatusNotFound)
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL+"/custom/v1", "sk-bad", "any", float64Ptr(0.7), intPtr(128))
	events, err := p.StreamChat(context.Background(), chat.ChatParams{Messages: []chat.Message{{Role: "user", Content: "hi"}}})
	if err != nil {
		t.Fatalf("StreamChat returned err: %v", err)
	}
	var got []chat.StreamEvent
	for e := range events {
		got = append(got, e)
	}
	if len(got) != 1 || got[0].Type != "RUN_ERROR" {
		t.Fatalf("expected single RUN_ERROR, got %+v", got)
	}
	for _, want := range []string{
		"404",
		upstream.URL + "/custom/v1/chat/completions",
		"x-error-code=InvalidEndpointOrModel.NotFound",
		"x-request-id=req-123",
		"model not found",
	} {
		if !strings.Contains(got[0].Error, want) {
			t.Fatalf("RUN_ERROR should mention %q, got %q", want, got[0].Error)
		}
	}
	if strings.Contains(got[0].Error, "sk-bad") {
		t.Fatalf("RUN_ERROR must not leak the API key, got %q", got[0].Error)
	}
}

func TestStreamChat4xxSanitizesEndpointDiagnostics(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, "denied", http.StatusForbidden)
	}))
	defer upstream.Close()

	p := openai.NewProvider(strings.Replace(upstream.URL, "://", "://user:pass@", 1), "sk-test", "any", float64Ptr(0.7), intPtr(128))
	events, err := p.StreamChat(context.Background(), chat.ChatParams{Messages: []chat.Message{{Role: "user", Content: "hi"}}})
	if err != nil {
		t.Fatalf("StreamChat returned err: %v", err)
	}
	var got []chat.StreamEvent
	for e := range events {
		got = append(got, e)
	}
	if len(got) != 1 || got[0].Type != "RUN_ERROR" {
		t.Fatalf("expected single RUN_ERROR, got %+v", got)
	}
	if strings.Contains(got[0].Error, "user:pass") {
		t.Fatalf("RUN_ERROR must not leak URL userinfo, got %q", got[0].Error)
	}
	if !strings.Contains(got[0].Error, upstream.URL+"/v1/chat/completions") {
		t.Fatalf("RUN_ERROR should mention status code, got %q", got[0].Error)
	}
}

func TestStreamChatSkipsMalformedChunks(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		flusher := w.(http.Flusher)
		for _, line := range []string{
			"data: {not-json}",
			`data: {"choices":[{"delta":{"content":"[EXPR:polite]ok"}}]}`,
			"data: [DONE]",
		} {
			fmt.Fprintf(w, "%s\n\n", line)
			flusher.Flush()
		}
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "any", float64Ptr(0.7), intPtr(128))
	events, err := p.StreamChat(context.Background(), chat.ChatParams{
		Messages:           []chat.Message{{Role: "user", Content: "hi"}},
		KnownExpressionIDs: []string{"polite"},
	})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}
	gotPolite := false
	for e := range events {
		if e.Type == "CUSTOM" && e.Value["expression"] == "polite" {
			gotPolite = true
		}
	}
	if !gotPolite {
		t.Fatal("expected polite expression after malformed chunk was skipped")
	}
}

func TestStreamChatReadErrorBecomesRunError(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/event-stream")
		w.Header().Set("Content-Length", "4096")
		w.WriteHeader(http.StatusOK)
		flusher := w.(http.Flusher)
		fmt.Fprint(w, "data: {\"choices\":[{\"delta\":{\"content\":\"partial\"}}]}\n\n")
		flusher.Flush()
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "any", float64Ptr(0.7), intPtr(128))
	events, err := p.StreamChat(context.Background(), chat.ChatParams{
		Messages: []chat.Message{{Role: "user", Content: "hi"}},
	})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}

	var got []chat.StreamEvent
	for e := range events {
		got = append(got, e)
	}

	hasRunError := false
	for _, e := range got {
		switch e.Type {
		case "RUN_ERROR":
			hasRunError = true
			if e.Error == "" {
				t.Fatalf("RUN_ERROR missing diagnostic: %+v", got)
			}
		case "TEXT_MESSAGE_END", "RUN_FINISHED":
			t.Fatalf("read error must not emit %s: %+v", e.Type, got)
		}
	}
	if !hasRunError {
		t.Fatalf("missing RUN_ERROR after stream read error: %+v", got)
	}
}

func TestPayloadLoggingFlagDoesNotAffectStreamEvents(t *testing.T) {
	t.Setenv("MILES_LOG_PAYLOADS", "1")
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/event-stream")
		fmt.Fprint(w, "data: {\"choices\":[{\"delta\":{\"content\":\"[EXPR:polite]secret text\"}}]}\n\n")
		fmt.Fprint(w, "data: [DONE]\n\n")
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "any", float64Ptr(0.7), intPtr(128))
	events, err := p.StreamChat(context.Background(), chat.ChatParams{
		Messages:           []chat.Message{{Role: "user", Content: "hi"}},
		KnownExpressionIDs: []string{"polite"},
	})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}
	seenText := false
	for event := range events {
		if event.Type == "TEXT_MESSAGE_CONTENT" && event.Delta == "secret text" {
			seenText = true
		}
	}
	if !seenText {
		t.Fatal("payload logging flag must not change stream parsing")
	}
}

func TestCompleteReturnsAssistantMessageContent(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var body struct {
			Messages []struct {
				Role    string `json:"role"`
				Content string `json:"content"`
			} `json:"messages"`
			Stream bool `json:"stream"`
		}
		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			t.Fatalf("upstream decode: %v", err)
		}
		if body.Stream {
			t.Fatalf("Complete request must not stream")
		}
		fmt.Fprint(w, `{"choices":[{"message":{"role":"assistant","content":"摘要文本"}}]}`)
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "test-model", float64Ptr(0.5), intPtr(256))
	got, err := p.Complete(context.Background(), chat.ChatParams{
		Messages: []chat.Message{
			{Role: "system", Content: "summary rules"},
			{Role: "user", Content: "请总结"},
		},
	})
	if err != nil {
		t.Fatalf("Complete: %v", err)
	}
	if got != "摘要文本" {
		t.Fatalf("Complete = %q, want %q", got, "摘要文本")
	}
}

func TestCompleteNonOKReturnsProviderError(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("X-Error-Code", "InvalidEndpointOrModel.NotFound")
		w.Header().Set("X-Request-Id", "req-complete-1")
		http.Error(w, `{"error":{"message":"model not found"}}`, http.StatusNotFound)
	}))
	defer upstream.Close()

	p := openai.NewProvider(strings.Replace(upstream.URL, "://", "://user:pass@", 1), "sk-bad", "test-model", float64Ptr(0.5), intPtr(256))
	_, err := p.Complete(context.Background(), chat.ChatParams{
		Messages: []chat.Message{{Role: "user", Content: "请总结"}},
	})
	if err == nil {
		t.Fatal("Complete error = nil, want provider error")
	}
	got := err.Error()
	for _, want := range []string{
		"404",
		upstream.URL + "/v1/chat/completions",
		"x-error-code=InvalidEndpointOrModel.NotFound",
		"x-request-id=req-complete-1",
		"model not found",
	} {
		if !strings.Contains(got, want) {
			t.Fatalf("Complete error should mention %q, got %q", want, got)
		}
	}
	for _, forbidden := range []string{"user:pass", "sk-bad"} {
		if strings.Contains(got, forbidden) {
			t.Fatalf("Complete error must not leak %q, got %q", forbidden, got)
		}
	}
}

func TestCompleteReturnsErrorForMissingContent(t *testing.T) {
	for _, tc := range []struct {
		name string
		body string
	}{
		{name: "empty choices", body: `{"choices":[]}`},
		{name: "blank content", body: `{"choices":[{"message":{"role":"assistant","content":"  \n\t  "}}]}`},
	} {
		t.Run(tc.name, func(t *testing.T) {
			upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				fmt.Fprint(w, tc.body)
			}))
			defer upstream.Close()

			p := openai.NewProvider(upstream.URL, "sk-test", "test-model", float64Ptr(0.5), intPtr(256))
			got, err := p.Complete(context.Background(), chat.ChatParams{
				Messages: []chat.Message{{Role: "user", Content: "请总结"}},
			})
			if err == nil {
				t.Fatalf("Complete error = nil with result %q, want error", got)
			}
			if !strings.Contains(err.Error(), "provider returned no completion content") {
				t.Fatalf("Complete error = %q, want no completion content", err.Error())
			}
		})
	}
}
