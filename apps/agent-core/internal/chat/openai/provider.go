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
	"sort"
	"strings"
	"time"

	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/expression"
	"milesedgeworth/agent-core/internal/mileslog"
)

type Provider struct {
	baseURL     string
	apiKey      string
	model       string
	temperature *float64
	maxTokens   *int
	httpClient  *http.Client
}

var logger = mileslog.New("MILES.CHAT.PROVIDER")

func NewProvider(baseURL, apiKey, model string, temperature *float64, maxTokens *int) *Provider {
	return &Provider{
		baseURL:     normalizeChatCompletionsURL(baseURL),
		apiKey:      apiKey,
		model:       model,
		temperature: cloneFloat64(temperature),
		maxTokens:   cloneInt(maxTokens),
		httpClient:  &http.Client{Timeout: 120 * time.Second},
	}
}

func cloneFloat64(value *float64) *float64 {
	if value == nil {
		return nil
	}
	clone := *value
	return &clone
}

func cloneInt(value *int) *int {
	if value == nil {
		return nil
	}
	clone := *value
	return &clone
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

type chatToolDefinition struct {
	Type     string `json:"type"`
	Function struct {
		Name        string          `json:"name"`
		Description string          `json:"description"`
		Parameters  json.RawMessage `json:"parameters"`
	} `json:"function"`
}

type chatMessage struct {
	Role             string          `json:"role"`
	Content          string          `json:"content,omitempty"`
	ReasoningContent string          `json:"reasoning_content,omitempty"`
	ToolCalls        []chat.ToolCall `json:"tool_calls,omitempty"`
	ToolCallID       string          `json:"tool_call_id,omitempty"`
}

type chatCompletionRequest struct {
	Model             string               `json:"model"`
	Messages          []chatMessage        `json:"messages"`
	Stream            bool                 `json:"stream"`
	Temperature       *float64             `json:"temperature,omitempty"`
	MaxTokens         *int                 `json:"max_tokens,omitempty"`
	Tools             []chatToolDefinition `json:"tools,omitempty"`
	ParallelToolCalls *bool                `json:"parallel_tool_calls,omitempty"`
}

type chatCompletionStreamChunk struct {
	Choices []struct {
		Delta struct {
			Content          string `json:"content"`
			ReasoningContent string `json:"reasoning_content"`
			ToolCalls        []struct {
				Index    int    `json:"index"`
				ID       string `json:"id"`
				Type     string `json:"type"`
				Function struct {
					Name      string `json:"name"`
					Arguments string `json:"arguments"`
				} `json:"function"`
			} `json:"tool_calls"`
		} `json:"delta"`
		FinishReason string `json:"finish_reason"`
	} `json:"choices"`
}

type chatCompletionResponse struct {
	Choices []struct {
		Message chatMessage `json:"message"`
	} `json:"choices"`
}

func (p *Provider) StreamChat(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
	events := make(chan chat.StreamEvent, 32)

	messages := makeChatMessages(params.Messages)
	body := chatCompletionRequest{
		Model:             p.model,
		Messages:          messages,
		Stream:            true,
		Temperature:       p.temperature,
		MaxTokens:         p.maxTokens,
		Tools:             makeChatTools(params.Tools),
		ParallelToolCalls: parallelToolCallsParam(params.Tools),
	}
	encoded, err := json.Marshal(body)
	if err != nil {
		close(events)
		return events, err
	}
	logger.Debug("request prepared",
		"model", p.model,
		"endpoint", sanitizeEndpoint(p.baseURL),
		"knownTags", len(params.KnownExpressionIDs),
		"messages", len(messages))

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

	runID := params.RunID
	if runID == "" {
		runID = "openai-run-1"
	}
	messageID := params.MessageID
	if messageID == "" {
		messageID = "openai-msg-1"
	}

	if resp.StatusCode != http.StatusOK {
		// Drain a small sample for diagnostics, then close.
		sample, _ := io.ReadAll(io.LimitReader(resp.Body, 512))
		resp.Body.Close()
		go func() {
			defer close(events)
			send(ctx, events, chat.StreamEvent{
				Type:  "RUN_ERROR",
				RunID: runID,
				Error: providerErrorMessage(resp, p.baseURL, sample),
			})
		}()
		return events, nil
	}

	knownTags := append([]string(nil), params.KnownExpressionIDs...)
	logger.Debug("stream started", "knownTags", strings.Join(knownTags, ","))

	go p.pipe(ctx, resp, events, runID, messageID, knownTags)
	return events, nil
}

func (p *Provider) Complete(ctx context.Context, params chat.ChatParams) (string, error) {
	body := chatCompletionRequest{
		Model:       p.model,
		Messages:    makeChatMessages(params.Messages),
		Stream:      false,
		Temperature: p.temperature,
		MaxTokens:   p.maxTokens,
	}
	encoded, err := json.Marshal(body)
	if err != nil {
		return "", err
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost,
		p.baseURL, bytes.NewReader(encoded))
	if err != nil {
		return "", err
	}
	httpReq.Header.Set("Content-Type", "application/json")
	httpReq.Header.Set("Authorization", "Bearer "+p.apiKey)

	resp, err := p.httpClient.Do(httpReq)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		sample, _ := io.ReadAll(io.LimitReader(resp.Body, 512))
		return "", fmt.Errorf("%s", providerErrorMessage(resp, p.baseURL, sample))
	}

	var completion chatCompletionResponse
	if err := json.NewDecoder(resp.Body).Decode(&completion); err != nil {
		return "", err
	}
	for _, choice := range completion.Choices {
		if content := strings.TrimSpace(choice.Message.Content); content != "" {
			return content, nil
		}
	}
	return "", fmt.Errorf("provider returned no completion content")
}

func makeChatMessages(messages []chat.Message) []chatMessage {
	out := make([]chatMessage, 0, len(messages))
	for _, message := range messages {
		out = append(out, chatMessage{
			Role:             message.Role,
			Content:          message.Content,
			ReasoningContent: message.ReasoningContent,
			ToolCalls:        message.ToolCalls,
			ToolCallID:       message.ToolCallID,
		})
	}
	return out
}

func makeChatTools(tools []chat.ToolDefinition) []chatToolDefinition {
	out := make([]chatToolDefinition, 0, len(tools))
	for _, tool := range tools {
		var item chatToolDefinition
		item.Type = "function"
		item.Function.Name = tool.Name
		item.Function.Description = tool.Description
		item.Function.Parameters = tool.Parameters
		out = append(out, item)
	}
	return out
}

func parallelToolCallsParam(tools []chat.ToolDefinition) *bool {
	if len(tools) == 0 {
		return nil
	}
	enabled := false
	return &enabled
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

func (p *Provider) pipe(ctx context.Context, resp *http.Response, events chan<- chat.StreamEvent, runID, messageID string, knownTags []string) {
	defer resp.Body.Close()
	defer close(events)

	sawFirstSpeaking := false
	textStarted := false
	ensureTextStarted := func() bool {
		if textStarted {
			return true
		}
		textStarted = true
		return send(ctx, events, chat.StreamEvent{
			Type:      "TEXT_MESSAGE_START",
			RunID:     runID,
			MessageID: messageID,
			Role:      "assistant",
		})
	}
	var pendingRawDelta strings.Builder
	takeRawDelta := func() string {
		raw := pendingRawDelta.String()
		pendingRawDelta.Reset()
		return raw
	}
	parser := expression.NewParser(
		knownTags,
		"neutral",
		func(text string) {
			rawDelta := takeRawDelta()
			if !ensureTextStarted() {
				return
			}
			if !sawFirstSpeaking {
				sawFirstSpeaking = true
				logger.Debug("fallback expression inserted",
					"expression", "neutral",
					"textLen", len([]rune(text)))
				send(ctx, events, chat.StreamEvent{
					Type:     "CUSTOM",
					Name:     "miles.pet.expression.requested",
					RunID:    runID,
					RawDelta: rawDelta,
					Value:    map[string]any{"state": "speaking", "expression": "neutral"},
				})
				rawDelta = ""
			}
			logger.Debug("text chunk parsed", "len", len([]rune(text)))
			send(ctx, events, chat.StreamEvent{
				Type:      "TEXT_MESSAGE_CONTENT",
				RunID:     runID,
				MessageID: messageID,
				Delta:     text,
				RawDelta:  rawDelta,
			})
		},
		func(tag string) {
			rawDelta := takeRawDelta()
			sawFirstSpeaking = true
			if !ensureTextStarted() {
				return
			}
			logger.Debug("expression tag parsed", "tag", tag)
			send(ctx, events, chat.StreamEvent{
				Type:     "CUSTOM",
				Name:     "miles.pet.expression.requested",
				RunID:    runID,
				RawDelta: rawDelta,
				Value:    map[string]any{"state": "speaking", "expression": tag},
			})
		},
	)

	type pendingToolCall struct {
		id        string
		name      strings.Builder
		arguments strings.Builder
	}
	pendingTools := map[int]*pendingToolCall{}
	var reasoningContent strings.Builder
	var providerRawOutput strings.Builder

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
		if providerRawOutput.Len() > 0 {
			providerRawOutput.WriteByte('\n')
		}
		providerRawOutput.WriteString(payload)
		var chunk chatCompletionStreamChunk
		if err := json.Unmarshal([]byte(payload), &chunk); err != nil {
			logger.Warn("malformed provider chunk skipped", "error", err)
			if mileslog.PayloadLoggingEnabled() {
				logger.Debug("malformed provider payload", "payload", payload)
			}
			continue
		}
		for _, choice := range chunk.Choices {
			if choice.Delta.ReasoningContent != "" {
				reasoningContent.WriteString(choice.Delta.ReasoningContent)
			}
			for _, tool := range choice.Delta.ToolCalls {
				item := pendingTools[tool.Index]
				if item == nil {
					item = &pendingToolCall{}
					pendingTools[tool.Index] = item
				}
				if tool.ID != "" {
					item.id = tool.ID
				}
				if tool.Function.Name != "" {
					item.name.WriteString(tool.Function.Name)
				}
				if tool.Function.Arguments != "" {
					item.arguments.WriteString(tool.Function.Arguments)
				}
			}
			if choice.Delta.Content != "" {
				logger.Debug("provider delta received", "len", len([]rune(choice.Delta.Content)))
				if mileslog.PayloadLoggingEnabled() {
					logger.Debug("provider delta payload", "text", choice.Delta.Content)
				}
				pendingRawDelta.WriteString(choice.Delta.Content)
				parser.Feed(choice.Delta.Content)
			}
		}
	}
	if err := scanner.Err(); err != nil {
		logger.Warn("provider stream read failed", "error", err)
		send(ctx, events, chat.StreamEvent{
			Type:  "RUN_ERROR",
			RunID: runID,
			Error: "provider stream read failed: " + err.Error(),
		})
		return
	}
	parser.Flush()
	logger.Debug("stream finished")

	rawOutput := providerRawOutput.String()
	if textStarted {
		if !send(ctx, events, chat.StreamEvent{
			Type:              "TEXT_MESSAGE_END",
			RunID:             runID,
			MessageID:         messageID,
			ProviderRawOutput: rawOutput,
		}) {
			return
		}
	}
	if len(pendingTools) == 0 {
		return
	}

	indexes := make([]int, 0, len(pendingTools))
	for index := range pendingTools {
		indexes = append(indexes, index)
	}
	sort.Ints(indexes)
	for _, index := range indexes {
		item := pendingTools[index]
		toolRawOutput := ""
		if index == indexes[0] && !textStarted {
			toolRawOutput = rawOutput
		}
		if !send(ctx, events, chat.StreamEvent{
			Type:              "TOOL_CALL",
			RunID:             runID,
			ToolCallID:        item.id,
			ToolName:          item.name.String(),
			ToolArgs:          item.arguments.String(),
			ReasoningContent:  reasoningContent.String(),
			ProviderRawOutput: toolRawOutput,
		}) {
			return
		}
	}
}

func send(ctx context.Context, events chan<- chat.StreamEvent, e chat.StreamEvent) bool {
	select {
	case <-ctx.Done():
		return false
	case events <- e:
		return true
	}
}
