package chat

import (
	"context"
	"encoding/json"
)

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

type ToolDefinition struct {
	Name        string          `json:"name"`
	Description string          `json:"description"`
	Parameters  json.RawMessage `json:"parameters"`
}

type ToolCallFunction struct {
	Name      string `json:"name"`
	Arguments string `json:"arguments"`
}

type ToolCall struct {
	ID       string           `json:"id"`
	Type     string           `json:"type"`
	Function ToolCallFunction `json:"function"`
}

type Message struct {
	Role             string     `json:"role"`
	Content          string     `json:"content"`
	ReasoningContent string     `json:"reasoning_content,omitempty"`
	ToolCalls        []ToolCall `json:"tool_calls,omitempty"`
	ToolCallID       string     `json:"tool_call_id,omitempty"`
}

type ChatParams struct {
	ConversationID     string
	RunID              string
	MessageID          string
	Operation          string
	Messages           []Message
	KnownExpressionIDs []string
	Tools              []ToolDefinition
	Continuation       bool
}

type StreamEvent struct {
	Type                 string         `json:"type"`
	Name                 string         `json:"name,omitempty"`
	RunID                string         `json:"runId,omitempty"`
	MessageID            string         `json:"messageId,omitempty"`
	Role                 string         `json:"role,omitempty"`
	Delta                string         `json:"delta,omitempty"`
	RawDelta             string         `json:"-"`
	Value                map[string]any `json:"value,omitempty"`
	Error                string         `json:"error,omitempty"`
	ToolCallID           string         `json:"toolCallId,omitempty"`
	ToolName             string         `json:"toolName,omitempty"`
	ToolArgs             string         `json:"toolArgs,omitempty"`
	ReasoningContent     string         `json:"-"`
	ProviderOutput       string         `json:"-"`
	ProviderStreamOutput string         `json:"-"`
}

type Provider interface {
	StreamChat(ctx context.Context, params ChatParams) (<-chan StreamEvent, error)
	Complete(ctx context.Context, params ChatParams) (string, error)
}
