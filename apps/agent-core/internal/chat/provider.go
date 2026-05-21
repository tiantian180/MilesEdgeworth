package chat

import "context"

// ExpressionInfo describes one expression tag the current skin makes available
// to the model. Sent from Qt to the sidecar with each chat request.
type ExpressionInfo struct {
	ID            string   `json:"id"`
	Label         string   `json:"label,omitempty"`
	Description   string   `json:"description,omitempty"`
	AllowedStates []string `json:"allowedStates,omitempty"`
}

type Request struct {
	ConversationID string           `json:"conversationId"`
	Message        string           `json:"message"`
	Expressions    []ExpressionInfo `json:"expressions,omitempty"`
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
