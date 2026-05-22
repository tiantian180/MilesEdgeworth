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

func TestBuildSystemPromptIncludesAllExpressions(t *testing.T) {
	exprs := []chat.ExpressionInfo{
		{ID: "neutral", Description: "默认状态"},
		{ID: "objection", Description: "强烈反驳"},
		{ID: "polite"},
	}
	prompt := openai.BuildSystemPrompt(exprs)

	for _, expect := range []string{
		"[EXPR:",
		"neutral",
		"默认状态",
		"objection",
		"强烈反驳",
		"polite",
		"只能使用方括号中列出的 id 原文",
		"[EXPR:objection]",
		"不要输出 [EXPR:异议]",
	} {
		if !strings.Contains(prompt, expect) {
			t.Fatalf("prompt missing %q\n---\n%s", expect, prompt)
		}
	}
}

func TestBuildSystemPromptHandlesEmptyExpressions(t *testing.T) {
	prompt := openai.BuildSystemPrompt(nil)
	// Still emits an instruction line, just without a tag list.
	if !strings.Contains(prompt, "[EXPR:") {
		t.Fatalf("prompt should explain markers even with no tags\n---\n%s", prompt)
	}
}

func TestStreamReplyHappyPath(t *testing.T) {
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
		if len(body.Messages) != 2 || body.Messages[0].Role != "system" {
			t.Fatalf("expected system+user messages, got %+v", body.Messages)
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

	p := openai.NewProvider(upstream.URL, "sk-test", "test-model", 0.5, 256)
	events, err := p.StreamReply(context.Background(), chat.Request{
		ConversationID: "c1",
		Message:        "你怎么看？",
		Expressions: []chat.ExpressionInfo{
			{ID: "neutral"},
			{ID: "objection"},
			{ID: "polite"},
		},
	})
	if err != nil {
		t.Fatalf("StreamReply: %v", err)
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

func TestStreamReplyAcceptsVersionedBaseURL(t *testing.T) {
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

	p := openai.NewProvider(upstream.URL+"/v1", "sk-test", "test-model", 0.5, 256)
	events, err := p.StreamReply(context.Background(), chat.Request{Message: "hi"})
	if err != nil {
		t.Fatalf("StreamReply: %v", err)
	}
	for range events {
	}
	if seenPath != "/v1/chat/completions" {
		t.Fatalf("requested path = %q, want /v1/chat/completions", seenPath)
	}
}

func TestStreamReplyAcceptsProviderVersionPrefix(t *testing.T) {
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

	p := openai.NewProvider(upstream.URL+"/api/v3", "sk-test", "test-model", 0.5, 256)
	events, err := p.StreamReply(context.Background(), chat.Request{Message: "hi"})
	if err != nil {
		t.Fatalf("StreamReply: %v", err)
	}
	for range events {
	}
	if seenPath != "/api/v3/chat/completions" {
		t.Fatalf("requested path = %q, want /api/v3/chat/completions", seenPath)
	}
}

func TestStreamReplyAcceptsFullChatCompletionsURL(t *testing.T) {
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

	p := openai.NewProvider(upstream.URL+"/custom/v1/chat/completions", "sk-test", "test-model", 0.5, 256)
	events, err := p.StreamReply(context.Background(), chat.Request{Message: "hi"})
	if err != nil {
		t.Fatalf("StreamReply: %v", err)
	}
	for range events {
	}
	if seenPath != "/custom/v1/chat/completions" {
		t.Fatalf("requested path = %q, want /custom/v1/chat/completions", seenPath)
	}
}

func TestStreamReply4xxBecomesRunError(t *testing.T) {
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

	p := openai.NewProvider(upstream.URL+"/custom/v1", "sk-bad", "any", 0.7, 128)
	events, err := p.StreamReply(context.Background(), chat.Request{Message: "hi"})
	if err != nil {
		t.Fatalf("StreamReply returned err: %v", err)
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

func TestStreamReply4xxSanitizesEndpointDiagnostics(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, "denied", http.StatusForbidden)
	}))
	defer upstream.Close()

	p := openai.NewProvider(strings.Replace(upstream.URL, "://", "://user:pass@", 1), "sk-test", "any", 0.7, 128)
	events, err := p.StreamReply(context.Background(), chat.Request{Message: "hi"})
	if err != nil {
		t.Fatalf("StreamReply returned err: %v", err)
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

func TestStreamReplySkipsMalformedChunks(t *testing.T) {
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

	p := openai.NewProvider(upstream.URL, "sk-test", "any", 0.7, 128)
	events, err := p.StreamReply(context.Background(), chat.Request{
		Message:     "hi",
		Expressions: []chat.ExpressionInfo{{ID: "polite"}},
	})
	if err != nil {
		t.Fatalf("StreamReply: %v", err)
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

func TestPayloadLoggingFlagDoesNotAffectStreamEvents(t *testing.T) {
	t.Setenv("MILES_LOG_PAYLOADS", "1")
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/event-stream")
		fmt.Fprint(w, "data: {\"choices\":[{\"delta\":{\"content\":\"[EXPR:polite]secret text\"}}]}\n\n")
		fmt.Fprint(w, "data: [DONE]\n\n")
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "any", 0.7, 128)
	events, err := p.StreamReply(context.Background(), chat.Request{
		Message:     "hi",
		Expressions: []chat.ExpressionInfo{{ID: "polite"}},
	})
	if err != nil {
		t.Fatalf("StreamReply: %v", err)
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
