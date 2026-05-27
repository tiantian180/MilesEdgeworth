package observability_test

import (
	"context"
	"strings"
	"testing"

	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/observability"

	"go.opentelemetry.io/otel/attribute"
	sdktrace "go.opentelemetry.io/otel/sdk/trace"
	"go.opentelemetry.io/otel/sdk/trace/tracetest"
)

type fakeProvider struct {
	streamEvents   chan chat.StreamEvent
	streamParams   chat.ChatParams
	completeText   string
	completeParams chat.ChatParams
}

func (p *fakeProvider) StreamChat(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
	_ = ctx
	p.streamParams = params
	if p.streamEvents != nil {
		return p.streamEvents, nil
	}
	events := make(chan chat.StreamEvent)
	close(events)
	return events, nil
}

func (p *fakeProvider) Complete(ctx context.Context, params chat.ChatParams) (string, error) {
	_ = ctx
	p.completeParams = params
	return p.completeText, nil
}

func float64Ptr(v float64) *float64 { return &v }

func intPtr(v int) *int { return &v }

func TestStreamChatCreatesLangfuseGenerationSpan(t *testing.T) {
	exporter := tracetest.NewInMemoryExporter()
	tracerProvider := sdktrace.NewTracerProvider(sdktrace.WithSyncer(exporter))
	t.Cleanup(func() { _ = tracerProvider.Shutdown(context.Background()) })

	events := make(chan chat.StreamEvent, 4)
	events <- chat.StreamEvent{Type: "RUN_STARTED", RunID: "run-1"}
	events <- chat.StreamEvent{
		Type:      "TEXT_MESSAGE_CONTENT",
		RunID:     "run-1",
		MessageID: "msg-1",
		Delta:     "reply",
		RawDelta:  "[EXPR:objection]reply",
	}
	events <- chat.StreamEvent{Type: "RUN_FINISHED", RunID: "run-1"}
	close(events)

	next := &fakeProvider{streamEvents: events}
	provider := observability.WrapProvider(next, observability.Options{
		TracerProvider: tracerProvider,
		Model:          "test-model",
		Temperature:    float64Ptr(0.4),
		MaxTokens:      intPtr(128),
		Environment:    "test",
		CaptureContent: true,
	})

	stream, err := provider.StreamChat(context.Background(), chat.ChatParams{
		ConversationID: "conv-1",
		RunID:          "run-1",
		MessageID:      "msg-1",
		Operation:      "chat",
		Messages: []chat.Message{
			{Role: "system", Content: "persona"},
			{Role: "user", Content: "hello"},
		},
	})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}
	for range stream {
	}

	spans := exporter.GetSpans()
	if len(spans) != 1 {
		t.Fatalf("span count = %d, want 1", len(spans))
	}
	span := spans[0]
	if span.Name != "miles.chat.stream" {
		t.Fatalf("span name = %q", span.Name)
	}
	attrs := spanAttributes(span.Attributes)
	for key, want := range map[string]string{
		"langfuse.session.id":                        "conv-1",
		"langfuse.observation.type":                  "generation",
		"langfuse.observation.model.name":            "test-model",
		"langfuse.observation.metadata.operation":    "chat",
		"langfuse.observation.metadata.run_id":       "run-1",
		"langfuse.observation.metadata.message_id":   "msg-1",
		"langfuse.observation.metadata.usage_source": "estimated",
		"deployment.environment.name":                "test",
	} {
		if got := attrs[key]; got != want {
			t.Fatalf("%s = %q, want %q", key, got, want)
		}
	}
	if !strings.Contains(attrs["langfuse.observation.input"], `"model":"test-model"`) ||
		!strings.Contains(attrs["langfuse.observation.input"], `"stream":true`) ||
		!strings.Contains(attrs["langfuse.observation.input"], `"messages"`) ||
		!strings.Contains(attrs["langfuse.observation.input"], "hello") {
		t.Fatalf("input should include raw provider request fields, got %q", attrs["langfuse.observation.input"])
	}
	if !strings.Contains(attrs["langfuse.observation.output"], "[EXPR:objection]reply") {
		t.Fatalf("output should include raw streamed reply, got %q", attrs["langfuse.observation.output"])
	}
	if !strings.Contains(attrs["langfuse.observation.model.parameters"], `"temperature":0.4`) {
		t.Fatalf("model parameters missing temperature: %q", attrs["langfuse.observation.model.parameters"])
	}
	if !strings.Contains(attrs["langfuse.observation.usage_details"], `"input"`) {
		t.Fatalf("usage details missing input estimate: %q", attrs["langfuse.observation.usage_details"])
	}
}

func TestStreamChatCapturesProviderRawOutputAndFullToolRequest(t *testing.T) {
	exporter := tracetest.NewInMemoryExporter()
	tracerProvider := sdktrace.NewTracerProvider(sdktrace.WithSyncer(exporter))
	t.Cleanup(func() { _ = tracerProvider.Shutdown(context.Background()) })

	rawOutput := `{"choices":[{"delta":{"reasoning_content":"需要移动。"}}]}` + "\n" +
		`{"choices":[{"delta":{"tool_calls":[{"index":0,"id":"tc-1","type":"function","function":{"name":"pet_motion","arguments":"{\"action\":\"moveTo\",\"x\":0.5,\"y\":0.5}"}}]},"finish_reason":"tool_calls"}]}`
	events := make(chan chat.StreamEvent, 1)
	events <- chat.StreamEvent{
		Type:              "TOOL_CALL",
		RunID:             "run-1",
		ToolCallID:        "tc-1",
		ToolName:          "pet_motion",
		ToolArgs:          `{"action":"moveTo","x":0.5,"y":0.5}`,
		ReasoningContent:  "需要移动。",
		ProviderRawOutput: rawOutput,
	}
	close(events)

	next := &fakeProvider{streamEvents: events}
	provider := observability.WrapProvider(next, observability.Options{
		TracerProvider: tracerProvider,
		Model:          "test-model",
		CaptureContent: true,
	})

	stream, err := provider.StreamChat(context.Background(), chat.ChatParams{
		ConversationID: "conv-1",
		RunID:          "run-1",
		MessageID:      "msg-1-tool-1",
		Operation:      "chat",
		Messages: []chat.Message{
			{Role: "user", Content: "走到中间"},
			{
				Role:             "assistant",
				Content:          "我先过去。",
				ReasoningContent: "需要移动。",
				ToolCalls: []chat.ToolCall{{
					ID:   "tc-1",
					Type: "function",
					Function: chat.ToolCallFunction{
						Name:      "pet_motion",
						Arguments: `{"action":"moveTo","x":0.5,"y":0.5}`,
					},
				}},
			},
			{Role: "tool", Content: `{"success":true}`, ToolCallID: "tc-1"},
		},
		Tools: []chat.ToolDefinition{{
			Name:        "pet_motion",
			Description: "move pet",
			Parameters:  []byte(`{"type":"object"}`),
		}},
	})
	if err != nil {
		t.Fatalf("StreamChat: %v", err)
	}
	for range stream {
	}

	spans := exporter.GetSpans()
	if len(spans) != 1 {
		t.Fatalf("span count = %d, want 1", len(spans))
	}
	attrs := spanAttributes(spans[0].Attributes)
	if attrs["langfuse.observation.output"] != rawOutput {
		t.Fatalf("output = %q, want raw provider output", attrs["langfuse.observation.output"])
	}
	input := attrs["langfuse.observation.input"]
	for _, want := range []string{
		`"tool_calls"`,
		`"tool_call_id":"tc-1"`,
		`"reasoning_content":"需要移动。"`,
		`"tools"`,
		`"parallel_tool_calls":false`,
	} {
		if !strings.Contains(input, want) {
			t.Fatalf("input missing %s: %q", want, input)
		}
	}
}

func TestStreamChatCallsForSameRunShareTraceID(t *testing.T) {
	exporter := tracetest.NewInMemoryExporter()
	tracerProvider := sdktrace.NewTracerProvider(sdktrace.WithSyncer(exporter))
	t.Cleanup(func() { _ = tracerProvider.Shutdown(context.Background()) })

	provider := observability.WrapProvider(&fakeProvider{}, observability.Options{
		TracerProvider: tracerProvider,
		Model:          "test-model",
		CaptureContent: true,
	})

	for _, messageID := range []string{"msg-1", "msg-1-tool-1"} {
		stream, err := provider.StreamChat(context.Background(), chat.ChatParams{
			ConversationID: "conv-1",
			RunID:          "run-1",
			MessageID:      messageID,
			Operation:      "chat",
			Messages:       []chat.Message{{Role: "user", Content: "走到中间"}},
		})
		if err != nil {
			t.Fatalf("StreamChat: %v", err)
		}
		for range stream {
		}
	}

	spans := exporter.GetSpans()
	if len(spans) != 2 {
		t.Fatalf("span count = %d, want 2", len(spans))
	}
	firstTraceID := spans[0].SpanContext.TraceID()
	secondTraceID := spans[1].SpanContext.TraceID()
	if firstTraceID != secondTraceID {
		t.Fatalf("trace ids differ: %s != %s", firstTraceID, secondTraceID)
	}
}

func TestCompleteCreatesLangfuseGenerationSpanWithoutContentWhenDisabled(t *testing.T) {
	exporter := tracetest.NewInMemoryExporter()
	tracerProvider := sdktrace.NewTracerProvider(sdktrace.WithSyncer(exporter))
	t.Cleanup(func() { _ = tracerProvider.Shutdown(context.Background()) })

	next := &fakeProvider{completeText: "summary"}
	provider := observability.WrapProvider(next, observability.Options{
		TracerProvider: tracerProvider,
		Model:          "test-model",
		CaptureContent: false,
	})

	got, err := provider.Complete(context.Background(), chat.ChatParams{
		ConversationID: "conv-1",
		RunID:          "run-2",
		MessageID:      "msg-2",
		Operation:      "summary",
		Messages: []chat.Message{
			{Role: "user", Content: "old private dialogue"},
		},
	})
	if err != nil {
		t.Fatalf("Complete: %v", err)
	}
	if got != "summary" {
		t.Fatalf("Complete = %q, want summary", got)
	}

	spans := exporter.GetSpans()
	if len(spans) != 1 {
		t.Fatalf("span count = %d, want 1", len(spans))
	}
	attrs := spanAttributes(spans[0].Attributes)
	if attrs["langfuse.observation.metadata.operation"] != "summary" {
		t.Fatalf("operation = %q, want summary", attrs["langfuse.observation.metadata.operation"])
	}
	if strings.Contains(attrs["langfuse.observation.input"], "old private dialogue") {
		t.Fatalf("input content should not be captured when disabled: %q", attrs["langfuse.observation.input"])
	}
	if strings.Contains(attrs["langfuse.observation.output"], "summary") {
		t.Fatalf("output content should not be captured when disabled: %q", attrs["langfuse.observation.output"])
	}
}

func spanAttributes(values []attribute.KeyValue) map[string]string {
	out := make(map[string]string, len(values))
	for _, value := range values {
		out[string(value.Key)] = value.Value.AsString()
	}
	return out
}
