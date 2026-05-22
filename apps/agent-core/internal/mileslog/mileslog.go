package mileslog

import (
	"context"
	"fmt"
	"io"
	"log/slog"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"time"
)

type sink struct {
	writer      io.Writer
	includeDate bool
}

type handler struct {
	slog.Handler
	mu    *sync.Mutex
	sinks []sink
	level slog.Level
	attrs []slog.Attr
}

var (
	defaultLogger *slog.Logger
	payloads      bool
)

func init() {
	payloads = payloadLoggingEnabledFromEnv()
	defaultLogger = slog.New(newHandler(defaultSinks(), parseLevel(os.Getenv("MILES_LOG_LEVEL"))))
}

func New(category string) *slog.Logger {
	if defaultLogger == nil {
		defaultLogger = slog.New(newHandler(defaultSinks(), slog.LevelInfo))
	}
	return defaultLogger.With("category", strings.ToUpper(strings.TrimSpace(category)))
}

func PayloadLoggingEnabled() bool {
	return payloads
}

func payloadLoggingEnabledFromEnv() bool {
	return os.Getenv("MILES_LOG_PAYLOADS") == "1"
}

func parseLevel(raw string) slog.Level {
	switch strings.ToLower(strings.TrimSpace(raw)) {
	case "debug":
		return slog.LevelDebug
	case "warn", "warning":
		return slog.LevelWarn
	case "error":
		return slog.LevelError
	default:
		return slog.LevelInfo
	}
}

func defaultSinks() []sink {
	sinks := []sink{{writer: os.Stderr, includeDate: false}}
	if path := strings.TrimSpace(os.Getenv("MILES_LOG_FILE")); path != "" {
		if file, err := os.OpenFile(path, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o600); err == nil {
			sinks = append(sinks, sink{writer: file, includeDate: true})
		}
	}
	return sinks
}

func newHandler(sinks []sink, level slog.Level) *handler {
	return &handler{
		Handler: slog.NewTextHandler(io.Discard, nil),
		mu:      &sync.Mutex{},
		sinks:   sinks,
		level:   level,
	}
}

func (h *handler) Enabled(_ context.Context, level slog.Level) bool {
	return level >= h.level
}

func (h *handler) Handle(_ context.Context, record slog.Record) error {
	attrs := make([]slog.Attr, 0, len(h.attrs)+record.NumAttrs())
	attrs = append(attrs, h.attrs...)
	record.Attrs(func(attr slog.Attr) bool {
		attrs = append(attrs, attr)
		return true
	})

	category := "MILES.APP"
	filtered := attrs[:0]
	for _, attr := range attrs {
		if attr.Key == "category" {
			if value := strings.TrimSpace(attr.Value.String()); value != "" {
				category = strings.ToUpper(value)
			}
			continue
		}
		filtered = append(filtered, attr)
	}

	source := sourceLocation(record.PC)
	h.mu.Lock()
	defer h.mu.Unlock()
	for _, sink := range h.sinks {
		line := formatLine(record.Time, record.Level, category, record.Message, filtered, source, sink.includeDate)
		if _, err := io.WriteString(sink.writer, line+"\n"); err != nil {
			return err
		}
	}
	return nil
}

func (h *handler) WithAttrs(attrs []slog.Attr) slog.Handler {
	next := *h
	next.attrs = append(append([]slog.Attr{}, h.attrs...), attrs...)
	return &next
}

func formatLine(t time.Time, level slog.Level, category string, message string, attrs []slog.Attr, source string, includeDate bool) string {
	if t.IsZero() {
		t = time.Now()
	}
	layout := "15:04:05.000"
	if includeDate {
		layout = "2006-01-02 15:04:05.000"
	}
	parts := []string{
		t.Format(layout),
		paddedLevel(level),
		"[" + category + "]",
		message,
	}
	for _, attr := range attrs {
		parts = append(parts, attr.Key+"="+attrValueString(attr.Value))
	}
	if source != "" {
		parts = append(parts, "["+source+"]")
	}
	return strings.Join(parts, " ")
}

func paddedLevel(level slog.Level) string {
	switch {
	case level <= slog.LevelDebug:
		return "DEBUG"
	case level < slog.LevelWarn:
		return "INFO "
	case level < slog.LevelError:
		return "WARN "
	default:
		return "ERROR"
	}
}

func attrValueString(value slog.Value) string {
	var text string
	switch value.Kind() {
	case slog.KindString:
		text = value.String()
	default:
		text = fmt.Sprint(value.Any())
	}
	if text == "" {
		return `""`
	}
	if strings.ContainsAny(text, " \t\r\n") {
		return strconv.Quote(text)
	}
	return text
}

func sourceLocation(pc uintptr) string {
	if pc == 0 {
		return ""
	}
	frames := runtime.CallersFrames([]uintptr{pc})
	frame, _ := frames.Next()
	if frame.File == "" {
		return ""
	}
	return filepath.Base(frame.File) + ":" + strconv.Itoa(frame.Line)
}
