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
	mu    *sync.Mutex
	sinks []sink
	level slog.Level
	attrs []slog.Attr
	group []string
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
		} else {
			fmt.Fprintf(os.Stderr, "mileslog: open log file %q: %v\n", path, err)
		}
	}
	return sinks
}

func newHandler(sinks []sink, level slog.Level) *handler {
	return &handler{
		mu:    &sync.Mutex{},
		sinks: sinks,
		level: level,
	}
}

func (h *handler) Enabled(_ context.Context, level slog.Level) bool {
	return level >= h.level
}

func (h *handler) Handle(_ context.Context, record slog.Record) error {
	attrs := make([]slog.Attr, 0, len(h.attrs)+record.NumAttrs())
	attrs = append(attrs, h.attrs...)
	record.Attrs(func(attr slog.Attr) bool {
		attrs = appendAttr(attrs, h.group, attr)
		return true
	})

	category := "MILES.APP"
	categoryFound := false
	filtered := attrs[:0]
	for _, attr := range attrs {
		if attr.Key == "category" {
			if !categoryFound {
				categoryFound = true
				if value := strings.TrimSpace(attr.Value.String()); value != "" {
					category = strings.ToUpper(value)
				}
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
	next.attrs = append([]slog.Attr{}, h.attrs...)
	next.attrs = appendAttrs(next.attrs, h.group, attrs)
	return &next
}

func (h *handler) WithGroup(name string) slog.Handler {
	if name == "" {
		return h
	}
	next := *h
	next.group = append(append([]string{}, h.group...), name)
	return &next
}

func appendAttrs(dst []slog.Attr, groups []string, attrs []slog.Attr) []slog.Attr {
	for _, attr := range attrs {
		dst = appendAttr(dst, groups, attr)
	}
	return dst
}

func appendAttr(dst []slog.Attr, groups []string, attr slog.Attr) []slog.Attr {
	attr.Value = attr.Value.Resolve()
	if attr.Equal(slog.Attr{}) {
		return dst
	}

	if attr.Value.Kind() == slog.KindGroup {
		groupAttrs := attr.Value.Group()
		if len(groupAttrs) == 0 {
			return dst
		}
		nextGroups := groups
		if attr.Key != "" {
			nextGroups = append(append([]string{}, groups...), attr.Key)
		}
		return appendAttrs(dst, nextGroups, groupAttrs)
	}

	if attr.Key == "" {
		return dst
	}
	attr.Key = qualifyKey(groups, attr.Key)
	return append(dst, attr)
}

func qualifyKey(groups []string, key string) string {
	if len(groups) == 0 {
		return key
	}
	parts := make([]string, 0, len(groups)+1)
	parts = append(parts, groups...)
	parts = append(parts, key)
	return strings.Join(parts, ".")
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
