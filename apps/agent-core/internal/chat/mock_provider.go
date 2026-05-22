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

func (p *MockProvider) StreamChat(ctx context.Context, params ChatParams) (<-chan StreamEvent, error) {
	events := make(chan StreamEvent)

	go func() {
		defer close(events)

		runID := params.RunID
		if runID == "" {
			runID = "mock-run-1"
		}
		messageID := params.MessageID
		if messageID == "" {
			messageID = "mock-message-1"
		}
		userText := lastUserMessage(params.Messages)
		reply := fmt.Sprintf("异议。你刚才说的是：%s。Phase 2.0 mock 链路已经接通。", userText)

		if !send(ctx, events, p.delay, StreamEvent{Type: "RUN_STARTED", RunID: runID}) {
			return
		}
		if !send(ctx, events, p.delay, StreamEvent{
			Type:  "CUSTOM",
			Name:  "miles.pet.expression.requested",
			RunID: runID,
			Value: map[string]any{
				"state":      "thinking",
				"expression": "neutral",
			},
		}) {
			return
		}
		if !send(ctx, events, p.delay, StreamEvent{
			Type:      "TEXT_MESSAGE_START",
			RunID:     runID,
			MessageID: messageID,
			Role:      "assistant",
		}) {
			return
		}
		if !send(ctx, events, p.delay, StreamEvent{
			Type:  "CUSTOM",
			Name:  "miles.pet.expression.requested",
			RunID: runID,
			Value: map[string]any{
				"state":      "speaking",
				"expression": "objection",
			},
		}) {
			return
		}

		for _, token := range splitReply(reply) {
			if !send(ctx, events, p.delay, StreamEvent{
				Type:      "TEXT_MESSAGE_CONTENT",
				RunID:     runID,
				MessageID: messageID,
				Delta:     token,
			}) {
				return
			}
		}

		if !send(ctx, events, p.delay, StreamEvent{
			Type:      "TEXT_MESSAGE_END",
			RunID:     runID,
			MessageID: messageID,
		}) {
			return
		}
		if !send(ctx, events, p.delay, StreamEvent{
			Type:  "CUSTOM",
			Name:  "miles.pet.expression.requested",
			RunID: runID,
			Value: map[string]any{
				"state":         "idle",
				"expression":    "neutral",
				"interruptHint": "afterCurrent",
			},
		}) {
			return
		}
		send(ctx, events, p.delay, StreamEvent{Type: "RUN_FINISHED", RunID: runID})
	}()

	return events, nil
}

func (p *MockProvider) Complete(ctx context.Context, params ChatParams) (string, error) {
	_ = ctx
	for i := len(params.Messages) - 1; i >= 0; i-- {
		if params.Messages[i].Role == "user" {
			return "摘要：" + params.Messages[i].Content, nil
		}
	}
	return "摘要：空对话", nil
}

func lastUserMessage(messages []Message) string {
	for i := len(messages) - 1; i >= 0; i-- {
		if messages[i].Role == "user" {
			return messages[i].Content
		}
	}
	return ""
}

func splitReply(reply string) []string {
	parts := strings.Fields(reply)
	if len(parts) == 0 {
		return []string{reply}
	}

	tokens := make([]string, 0, len(parts))
	for i, part := range parts {
		if i < len(parts)-1 {
			part += " "
		}
		tokens = append(tokens, part)
	}
	return tokens
}

func send(ctx context.Context, events chan<- StreamEvent, delay time.Duration, event StreamEvent) bool {
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
	case events <- event:
		return true
	}
}
