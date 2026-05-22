package mileslog

import (
	"bytes"
	"context"
	"log/slog"
	"strings"
	"testing"
)

func TestHandlerFormatsCategoryMessageAttrsAndSource(t *testing.T) {
	var buf bytes.Buffer
	h := newHandler([]sink{{writer: &buf, includeDate: false}}, slog.LevelDebug)
	logger := slog.New(h).With("category", "MILES.TEST")

	logger.Debug("stream event", "deltaLen", 3, "state", "speaking")

	got := buf.String()
	for _, want := range []string{
		"DEBUG",
		"[MILES.TEST]",
		"stream event",
		"deltaLen=3",
		"state=speaking",
		"[mileslog_test.go:",
	} {
		if !strings.Contains(got, want) {
			t.Fatalf("formatted log missing %q in %q", want, got)
		}
	}
	if strings.Contains(got, "time=") || strings.Contains(got, "msg=") {
		t.Fatalf("custom handler must not use slog TextHandler format: %q", got)
	}
}

func TestHandlerFiltersBelowConfiguredLevel(t *testing.T) {
	var buf bytes.Buffer
	h := newHandler([]sink{{writer: &buf, includeDate: false}}, slog.LevelInfo)
	logger := slog.New(h).With("category", "MILES.TEST")

	logger.Debug("hidden")
	logger.Info("shown")

	got := buf.String()
	if strings.Contains(got, "hidden") {
		t.Fatalf("debug log should be filtered at info level: %q", got)
	}
	if !strings.Contains(got, "shown") {
		t.Fatalf("info log should be emitted: %q", got)
	}
}

func TestHandlerAddsDateForFileSink(t *testing.T) {
	var buf bytes.Buffer
	h := newHandler([]sink{{writer: &buf, includeDate: true}}, slog.LevelInfo)
	logger := slog.New(h).With("category", "MILES.TEST")

	logger.Info("file line")

	got := buf.String()
	if len(got) < len("2006-01-02 15:04:05.000") || got[4] != '-' || got[13] != ':' {
		t.Fatalf("file sink should include full date timestamp, got %q", got)
	}
}

func TestPayloadFlagParser(t *testing.T) {
	t.Setenv("MILES_LOG_PAYLOADS", "1")
	if !payloadLoggingEnabledFromEnv() {
		t.Fatal("MILES_LOG_PAYLOADS=1 should enable payload logging")
	}
	t.Setenv("MILES_LOG_PAYLOADS", "0")
	if payloadLoggingEnabledFromEnv() {
		t.Fatal("MILES_LOG_PAYLOADS=0 should disable payload logging")
	}
}

func TestEnabledHonorsLevel(t *testing.T) {
	h := newHandler([]sink{{writer: &bytes.Buffer{}, includeDate: false}}, slog.LevelWarn)
	if h.Enabled(context.Background(), slog.LevelInfo) {
		t.Fatal("info should be disabled at warn level")
	}
	if !h.Enabled(context.Background(), slog.LevelError) {
		t.Fatal("error should be enabled at warn level")
	}
}
