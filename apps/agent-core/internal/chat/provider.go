package chat

import "context"

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
	PersonaPrompt  string           `json:"personaPrompt,omitempty"`
}

type Message struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

type ChatParams struct {
	RunID              string
	MessageID          string
	Messages           []Message
	KnownExpressionIDs []string
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
	StreamChat(ctx context.Context, params ChatParams) (<-chan StreamEvent, error)
	Complete(ctx context.Context, params ChatParams) (string, error)
}
