package observability

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"strings"
	"unicode/utf8"

	"milesedgeworth/agent-core/internal/chat"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/trace"
)

type Options struct {
	TracerProvider trace.TracerProvider
	Model          string
	Temperature    *float64
	MaxTokens      *int
	Environment    string
	CaptureContent bool
}

type tracedProvider struct {
	next   chat.Provider
	tracer trace.Tracer
	opts   Options
}

func WrapProvider(next chat.Provider, opts Options) chat.Provider {
	if next == nil {
		return nil
	}
	tracerProvider := opts.TracerProvider
	if tracerProvider == nil {
		tracerProvider = otel.GetTracerProvider()
	}
	return &tracedProvider{
		next:   next,
		tracer: tracerProvider.Tracer("milesedgeworth/agent-core/chat"),
		opts:   opts,
	}
}

func (p *tracedProvider) StreamChat(ctx context.Context, params chat.ChatParams) (<-chan chat.StreamEvent, error) {
	ctx, span := p.startGenerationSpan(ctx, "miles.chat.stream", params)
	events, err := p.next.StreamChat(ctx, params)
	if err != nil {
		recordSpanError(span, err)
		span.End()
		return events, err
	}

	out := make(chan chat.StreamEvent, 32)
	go func() {
		defer close(out)
		defer span.End()

		var output strings.Builder
		var providerError string
		for event := range events {
			if event.Type == "TEXT_MESSAGE_CONTENT" {
				output.WriteString(event.Delta)
			}
			if event.Type == "RUN_ERROR" {
				providerError = event.Error
				span.SetStatus(codes.Error, event.Error)
				span.SetAttributes(
					attribute.String("langfuse.observation.level", "ERROR"),
					attribute.String("langfuse.observation.status_message", event.Error),
				)
			}
			out <- event
		}

		p.finishGenerationSpan(span, params, output.String())
		if providerError == "" {
			span.SetStatus(codes.Ok, "")
		}
	}()
	return out, nil
}

func (p *tracedProvider) Complete(ctx context.Context, params chat.ChatParams) (string, error) {
	ctx, span := p.startGenerationSpan(ctx, "miles.chat.complete", params)
	output, err := p.next.Complete(ctx, params)
	if err != nil {
		recordSpanError(span, err)
		span.End()
		return "", err
	}
	p.finishGenerationSpan(span, params, output)
	span.SetStatus(codes.Ok, "")
	span.End()
	return output, nil
}

func (p *tracedProvider) startGenerationSpan(ctx context.Context, name string, params chat.ChatParams) (context.Context, trace.Span) {
	ctx, span := p.tracer.Start(ctx, name, trace.WithSpanKind(trace.SpanKindClient))
	attrs := []attribute.KeyValue{
		attribute.String("langfuse.trace.name", name),
		attribute.String("langfuse.observation.type", "generation"),
		attribute.String("langfuse.observation.model.name", p.opts.Model),
		attribute.String("langfuse.observation.model.parameters", jsonString(modelParameters(p.opts))),
		attribute.String("langfuse.observation.metadata.operation", operationName(params.Operation)),
		attribute.String("langfuse.observation.metadata.run_id", params.RunID),
		attribute.String("langfuse.observation.metadata.message_id", params.MessageID),
		attribute.String("langfuse.observation.metadata.usage_source", "estimated"),
		attribute.String("langfuse.observation.metadata.system_prompt_hash", systemPromptHash(params.Messages)),
		attribute.StringSlice("langfuse.trace.tags", []string{"miles-edgeworth", operationName(params.Operation)}),
	}
	if sessionID := safeSessionID(params.ConversationID); sessionID != "" {
		attrs = append(attrs, attribute.String("langfuse.session.id", sessionID))
	}
	if strings.TrimSpace(p.opts.Environment) != "" {
		attrs = append(attrs, attribute.String("deployment.environment.name", strings.TrimSpace(p.opts.Environment)))
	}
	if p.opts.CaptureContent {
		attrs = append(attrs,
			attribute.String("langfuse.trace.input", lastUserMessage(params.Messages)),
			attribute.String("langfuse.observation.input", jsonString(messagesForTrace(params.Messages))),
		)
	}
	span.SetAttributes(attrs...)
	return ctx, span
}

func (p *tracedProvider) finishGenerationSpan(span trace.Span, params chat.ChatParams, output string) {
	attrs := []attribute.KeyValue{
		attribute.String("langfuse.observation.usage_details", jsonString(usageDetails(params.Messages, output))),
	}
	if p.opts.CaptureContent {
		attrs = append(attrs,
			attribute.String("langfuse.trace.output", output),
			attribute.String("langfuse.observation.output", output),
		)
	} else {
		attrs = append(attrs,
			attribute.Int("langfuse.observation.metadata.input_chars", messageChars(params.Messages)),
			attribute.Int("langfuse.observation.metadata.output_chars", utf8.RuneCountInString(output)),
		)
	}
	span.SetAttributes(attrs...)
}

func recordSpanError(span trace.Span, err error) {
	span.RecordError(err)
	span.SetStatus(codes.Error, err.Error())
	span.SetAttributes(
		attribute.String("langfuse.observation.level", "ERROR"),
		attribute.String("langfuse.observation.status_message", err.Error()),
	)
}

func modelParameters(opts Options) map[string]any {
	params := map[string]any{}
	if opts.Temperature != nil {
		params["temperature"] = *opts.Temperature
	}
	if opts.MaxTokens != nil {
		params["max_tokens"] = *opts.MaxTokens
	}
	return params
}

func messagesForTrace(messages []chat.Message) []map[string]string {
	out := make([]map[string]string, 0, len(messages))
	for _, message := range messages {
		out = append(out, map[string]string{
			"role":    message.Role,
			"content": message.Content,
		})
	}
	return out
}

func usageDetails(messages []chat.Message, output string) map[string]int {
	input := estimatedTokensForMessages(messages)
	outputTokens := estimateTokens(output)
	return map[string]int{
		"input":  input,
		"output": outputTokens,
		"total":  input + outputTokens,
	}
}

func estimatedTokensForMessages(messages []chat.Message) int {
	total := 0
	for _, message := range messages {
		total += estimateTokens(message.Role)
		total += estimateTokens(message.Content)
	}
	return total
}

func estimateTokens(text string) int {
	return utf8.RuneCountInString(text) * 2
}

func messageChars(messages []chat.Message) int {
	total := 0
	for _, message := range messages {
		total += utf8.RuneCountInString(message.Content)
	}
	return total
}

func jsonString(value any) string {
	encoded, err := json.Marshal(value)
	if err != nil {
		return "{}"
	}
	return string(encoded)
}

func operationName(operation string) string {
	trimmed := strings.TrimSpace(operation)
	if trimmed == "" {
		return "chat"
	}
	return trimmed
}

func lastUserMessage(messages []chat.Message) string {
	for i := len(messages) - 1; i >= 0; i-- {
		if messages[i].Role == "user" {
			return messages[i].Content
		}
	}
	return ""
}

func systemPromptHash(messages []chat.Message) string {
	for _, message := range messages {
		if message.Role == "system" && strings.TrimSpace(message.Content) != "" {
			sum := sha256.Sum256([]byte(message.Content))
			return hex.EncodeToString(sum[:8])
		}
	}
	return ""
}

func safeSessionID(value string) string {
	trimmed := strings.TrimSpace(value)
	if len(trimmed) == 0 || len(trimmed) >= 200 {
		return ""
	}
	for _, r := range trimmed {
		if r < 0x20 || r > 0x7e {
			return ""
		}
	}
	return trimmed
}
