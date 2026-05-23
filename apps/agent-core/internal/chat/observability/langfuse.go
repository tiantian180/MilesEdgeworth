package observability

import (
	"context"
	"encoding/base64"
	"net/url"
	"strings"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracehttp"
	"go.opentelemetry.io/otel/sdk/resource"
	sdktrace "go.opentelemetry.io/otel/sdk/trace"
	semconv "go.opentelemetry.io/otel/semconv/v1.27.0"
)

type LangfuseOTLPConfig struct {
	Host      string
	PublicKey string
	SecretKey string
}

func NewLangfuseTracerProvider(ctx context.Context, cfg LangfuseOTLPConfig) (*sdktrace.TracerProvider, error) {
	auth := base64.StdEncoding.EncodeToString([]byte(cfg.PublicKey + ":" + cfg.SecretKey))
	exporter, err := otlptracehttp.New(ctx,
		otlptracehttp.WithEndpointURL(langfuseTracesEndpoint(cfg.Host)),
		otlptracehttp.WithHeaders(map[string]string{
			"Authorization":                "Basic " + auth,
			"x-langfuse-ingestion-version": "4",
		}),
	)
	if err != nil {
		return nil, err
	}

	tracerProvider := sdktrace.NewTracerProvider(
		sdktrace.WithBatcher(exporter),
		sdktrace.WithResource(resource.NewWithAttributes(
			semconv.SchemaURL,
			semconv.ServiceName("miles-agent"),
		)),
	)
	otel.SetTracerProvider(tracerProvider)
	return tracerProvider, nil
}

func langfuseTracesEndpoint(host string) string {
	trimmed := strings.TrimRight(strings.TrimSpace(host), "/")
	if trimmed == "" {
		return trimmed
	}
	parsed, err := url.Parse(trimmed)
	if err != nil {
		return trimmed + "/api/public/otel/v1/traces"
	}
	path := strings.TrimRight(parsed.Path, "/")
	switch {
	case strings.HasSuffix(path, "/api/public/otel/v1/traces"):
		return trimmed
	case strings.HasSuffix(path, "/api/public/otel"):
		return trimmed + "/v1/traces"
	default:
		return trimmed + "/api/public/otel/v1/traces"
	}
}
