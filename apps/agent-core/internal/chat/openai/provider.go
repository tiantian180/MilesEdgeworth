// Package openai implements an OpenAI-compatible Chat Completions provider.
package openai

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"

	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/expression"
)

type Provider struct {
	baseURL     string
	apiKey      string
	model       string
	temperature float64
	maxTokens   int
	httpClient  *http.Client
}

func NewProvider(baseURL, apiKey, model string, temperature float64, maxTokens int) *Provider {
	return &Provider{
		baseURL:     normalizeChatCompletionsURL(baseURL),
		apiKey:      apiKey,
		model:       model,
		temperature: temperature,
		maxTokens:   maxTokens,
		httpClient:  &http.Client{Timeout: 120 * time.Second},
	}
}

func normalizeChatCompletionsURL(raw string) string {
	trimmed := strings.TrimRight(strings.TrimSpace(raw), "/")
	if trimmed == "" {
		return trimmed
	}

	parsed, err := url.Parse(trimmed)
	if err != nil {
		return trimmed + "/v1/chat/completions"
	}

	path := strings.TrimRight(parsed.Path, "/")
	switch {
	case path == "":
		return trimmed + "/v1/chat/completions"
	case strings.HasSuffix(path, "/chat/completions"):
		return trimmed
	default:
		return trimmed + "/chat/completions"
	}
}

// BuildSystemPrompt assembles the persona + expression instruction shown to the model.
// Phase 2.3 will replace this with the full Miles persona; Phase 2.1 only ships the
// minimal instruction needed for the [EXPR:tag] protocol to function.
func BuildSystemPrompt(expressions []chat.ExpressionInfo) string {
	return buildSystemPrompt(expressions)
}

func buildSystemPrompt(expressions []chat.ExpressionInfo) string {
	var sb strings.Builder
	sb.WriteString("你是 Miles Edgeworth 桌宠助手。回复时在每段文字开头用 [EXPR:id] 标记当前表达。")
	sb.WriteString("只能使用方括号中列出的 id 原文，不要翻译 id，也不要使用中文 label。")
	sb.WriteString("例如使用 [EXPR:objection]，不要输出 [EXPR:异议]。")
	sb.WriteString("情绪延续时不重复标记。回复的第一段文字必须有标记。\n\n")
	if len(expressions) == 0 {
		return sb.String()
	}
	sb.WriteString("当前可用表达标签：\n")
	for _, e := range expressions {
		sb.WriteString("- ")
		sb.WriteString(e.ID)
		if e.Description != "" {
			sb.WriteString("：")
			sb.WriteString(e.Description)
		}
		sb.WriteString("\n")
	}
	return sb.String()
}

type chatMessage struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

type chatCompletionRequest struct {
	Model       string        `json:"model"`
	Messages    []chatMessage `json:"messages"`
	Stream      bool          `json:"stream"`
	Temperature float64       `json:"temperature,omitempty"`
	MaxTokens   int           `json:"max_tokens,omitempty"`
}

type chatCompletionStreamChunk struct {
	Choices []struct {
		Delta struct {
			Content string `json:"content"`
		} `json:"delta"`
	} `json:"choices"`
}

const (
	runID     = "openai-run-1"
	messageID = "openai-msg-1"
)

func (p *Provider) StreamReply(ctx context.Context, req chat.Request) (<-chan chat.StreamEvent, error) {
	events := make(chan chat.StreamEvent, 32)

	body := chatCompletionRequest{
		Model: p.model,
		Messages: []chatMessage{
			{Role: "system", Content: BuildSystemPrompt(req.Expressions)},
			{Role: "user", Content: req.Message},
		},
		Stream:      true,
		Temperature: p.temperature,
		MaxTokens:   p.maxTokens,
	}
	encoded, err := json.Marshal(body)
	if err != nil {
		close(events)
		return events, err
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost,
		p.baseURL, bytes.NewReader(encoded))
	if err != nil {
		close(events)
		return events, err
	}
	httpReq.Header.Set("Content-Type", "application/json")
	httpReq.Header.Set("Authorization", "Bearer "+p.apiKey)
	httpReq.Header.Set("Accept", "text/event-stream")

	resp, err := p.httpClient.Do(httpReq)
	if err != nil {
		close(events)
		return events, err
	}

	if resp.StatusCode != http.StatusOK {
		// Drain a small sample for diagnostics, then close.
		sample, _ := io.ReadAll(io.LimitReader(resp.Body, 512))
		resp.Body.Close()
		go func() {
			defer close(events)
			send(ctx, events, chat.StreamEvent{
				Type:  "RUN_ERROR",
				Error: providerErrorMessage(resp, p.baseURL, sample),
			})
		}()
		return events, nil
	}

	knownTags := make([]string, 0, len(req.Expressions))
	for _, e := range req.Expressions {
		knownTags = append(knownTags, e.ID)
	}

	go p.pipe(ctx, resp, events, knownTags)
	return events, nil
}

func providerErrorMessage(resp *http.Response, endpoint string, sample []byte) string {
	message := fmt.Sprintf("provider returned %d from %s", resp.StatusCode, sanitizeEndpoint(endpoint))
	var diagnostics []string
	if errorCode := strings.TrimSpace(resp.Header.Get("X-Error-Code")); errorCode != "" {
		diagnostics = append(diagnostics, "x-error-code="+errorCode)
	}
	if requestID := strings.TrimSpace(resp.Header.Get("X-Request-Id")); requestID != "" {
		diagnostics = append(diagnostics, "x-request-id="+requestID)
	}
	if len(diagnostics) > 0 {
		message += " (" + strings.Join(diagnostics, ", ") + ")"
	}
	if body := strings.TrimSpace(string(sample)); body != "" {
		message += ": " + body
	}
	return message
}

func sanitizeEndpoint(endpoint string) string {
	parsed, err := url.Parse(endpoint)
	if err != nil {
		return endpoint
	}
	parsed.User = nil
	parsed.RawQuery = ""
	parsed.Fragment = ""
	return parsed.String()
}

func (p *Provider) pipe(ctx context.Context, resp *http.Response, events chan<- chat.StreamEvent, knownTags []string) {
	defer resp.Body.Close()
	defer close(events)

	if !send(ctx, events, chat.StreamEvent{Type: "RUN_STARTED", RunID: runID}) {
		return
	}
	if !send(ctx, events, chat.StreamEvent{
		Type:  "CUSTOM",
		Name:  "miles.pet.expression.requested",
		RunID: runID,
		Value: map[string]any{"state": "thinking", "expression": "neutral"},
	}) {
		return
	}
	if !send(ctx, events, chat.StreamEvent{
		Type:      "TEXT_MESSAGE_START",
		RunID:     runID,
		MessageID: messageID,
		Role:      "assistant",
	}) {
		return
	}

	sawFirstSpeaking := false
	parser := expression.NewParser(
		knownTags,
		"neutral",
		func(text string) {
			if !sawFirstSpeaking {
				sawFirstSpeaking = true
				send(ctx, events, chat.StreamEvent{
					Type:  "CUSTOM",
					Name:  "miles.pet.expression.requested",
					RunID: runID,
					Value: map[string]any{"state": "speaking", "expression": "neutral"},
				})
			}
			send(ctx, events, chat.StreamEvent{
				Type:      "TEXT_MESSAGE_CONTENT",
				RunID:     runID,
				MessageID: messageID,
				Delta:     text,
			})
		},
		func(tag string) {
			sawFirstSpeaking = true
			send(ctx, events, chat.StreamEvent{
				Type:  "CUSTOM",
				Name:  "miles.pet.expression.requested",
				RunID: runID,
				Value: map[string]any{"state": "speaking", "expression": tag},
			})
		},
	)

	scanner := bufio.NewScanner(resp.Body)
	scanner.Buffer(make([]byte, 0, 64*1024), 1024*1024)
	for scanner.Scan() {
		line := scanner.Text()
		if !strings.HasPrefix(line, "data:") {
			continue
		}
		payload := strings.TrimSpace(strings.TrimPrefix(line, "data:"))
		if payload == "[DONE]" {
			break
		}
		var chunk chatCompletionStreamChunk
		if err := json.Unmarshal([]byte(payload), &chunk); err != nil {
			continue
		}
		for _, choice := range chunk.Choices {
			if choice.Delta.Content != "" {
				parser.Feed(choice.Delta.Content)
			}
		}
	}
	parser.Flush()

	send(ctx, events, chat.StreamEvent{
		Type:      "TEXT_MESSAGE_END",
		RunID:     runID,
		MessageID: messageID,
	})
	send(ctx, events, chat.StreamEvent{
		Type:  "CUSTOM",
		Name:  "miles.pet.expression.requested",
		RunID: runID,
		Value: map[string]any{
			"state":         "idle",
			"expression":    "neutral",
			"interruptHint": "afterCurrent",
		},
	})
	send(ctx, events, chat.StreamEvent{Type: "RUN_FINISHED", RunID: runID})
}

func send(ctx context.Context, events chan<- chat.StreamEvent, e chat.StreamEvent) bool {
	select {
	case <-ctx.Done():
		return false
	case events <- e:
		return true
	}
}
