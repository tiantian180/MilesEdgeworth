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
