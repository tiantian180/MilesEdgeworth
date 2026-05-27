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
	CaptureSSE     bool
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
	ctx, span := p.startGenerationSpan(ctx, "miles.chat.stream", params, true)
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
		var rawOutput strings.Builder
		var providerOutput string
		var providerStreamOutput string
		var providerError string
		for event := range events {
			if event.ProviderOutput != "" {
				providerOutput = event.ProviderOutput
			}
			if event.ProviderStreamOutput != "" {
				providerStreamOutput = event.ProviderStreamOutput
			}
			if event.RawDelta != "" {
				rawOutput.WriteString(event.RawDelta)
			}
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

		recordedOutput := providerOutput
		if p.opts.CaptureSSE && providerStreamOutput != "" {
			recordedOutput = providerStreamOutput
		}
		if recordedOutput == "" && rawOutput.Len() > 0 {
			recordedOutput = rawOutput.String()
		}
		if recordedOutput == "" {
			recordedOutput = output.String()
		}
		p.finishGenerationSpan(span, params, recordedOutput)
		if providerError == "" {
			span.SetStatus(codes.Ok, "")
		}
	}()
	return out, nil
}

func (p *tracedProvider) Complete(ctx context.Context, params chat.ChatParams) (string, error) {
	ctx, span := p.startGenerationSpan(ctx, "miles.chat.complete", params, false)
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

func (p *tracedProvider) startGenerationSpan(ctx context.Context, name string, params chat.ChatParams, stream bool) (context.Context, trace.Span) {
	ctx = contextWithRunTrace(ctx, params.RunID)
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
		input := jsonString(requestPayloadForTrace(p.opts, params, stream))
		attrs = append(attrs,
			attribute.String("langfuse.trace.input", input),
			attribute.String("langfuse.observation.input", input),
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

func contextWithRunTrace(ctx context.Context, runID string) context.Context {
	if strings.TrimSpace(runID) == "" || trace.SpanContextFromContext(ctx).IsValid() {
		return ctx
	}
	sum := sha256.Sum256([]byte("milesedgeworth-run:" + runID))
	var traceID trace.TraceID
	copy(traceID[:], sum[:16])
	var spanID trace.SpanID
	copy(spanID[:], sum[16:24])
	if !traceID.IsValid() || !spanID.IsValid() {
		return ctx
	}
	spanContext := trace.NewSpanContext(trace.SpanContextConfig{
		TraceID:    traceID,
		SpanID:     spanID,
		TraceFlags: trace.FlagsSampled,
		Remote:     true,
	})
	return trace.ContextWithRemoteSpanContext(ctx, spanContext)
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

func requestPayloadForTrace(opts Options, params chat.ChatParams, stream bool) map[string]any {
	payload := map[string]any{
		"messages": messagesForTrace(params.Messages),
		"model":    opts.Model,
		"stream":   stream,
	}
	if len(params.Tools) > 0 {
		payload["tools"] = toolsForTrace(params.Tools)
		payload["parallel_tool_calls"] = false
	}
	if opts.Temperature != nil {
		payload["temperature"] = *opts.Temperature
	}
	if opts.MaxTokens != nil {
		payload["max_tokens"] = *opts.MaxTokens
	}
	return payload
}

func messagesForTrace(messages []chat.Message) []map[string]any {
	out := make([]map[string]any, 0, len(messages))
	for _, message := range messages {
		item := map[string]any{"role": message.Role}
		if message.Content != "" {
			item["content"] = message.Content
		}
		if message.ReasoningContent != "" {
			item["reasoning_content"] = message.ReasoningContent
		}
		if len(message.ToolCalls) > 0 {
			item["tool_calls"] = message.ToolCalls
		}
		if message.ToolCallID != "" {
			item["tool_call_id"] = message.ToolCallID
		}
		out = append(out, item)
	}
	return out
}

func toolsForTrace(tools []chat.ToolDefinition) []map[string]any {
	out := make([]map[string]any, 0, len(tools))
	for _, tool := range tools {
		out = append(out, map[string]any{
			"type": "function",
			"function": map[string]any{
				"name":        tool.Name,
				"description": tool.Description,
				"parameters":  tool.Parameters,
			},
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
