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

func TestStreamReply4xxBecomesRunError(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, `{"error":{"message":"invalid api key"}}`, http.StatusUnauthorized)
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-bad", "any", 0.7, 128)
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
	if !strings.Contains(got[0].Error, "401") {
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
