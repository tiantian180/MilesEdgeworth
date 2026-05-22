# 日志规范落地 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the unified Qt + Go logging standard defined in `docs/v2/设计方案/日志规范设计.md`, replacing ad-hoc debug plumbing with categorized, filtered, safe, file-capable logs.

**Architecture:** Qt owns a small `logging` module that installs a custom `qInstallMessageHandler`, formats only `miles.*` categories, writes stderr, and optionally writes a truncated-per-run log file. Go sidecar owns a small `internal/mileslog` package that initializes itself from env vars, exposes `mileslog.New("MILES.*")`, formats records through a custom `slog.Handler`, supports optional payload logging, and writes stderr plus optional append-mode file output. ChatController forwards sidecar stderr directly and passes logging env vars through `QProcessEnvironment`.

**Tech Stack:** Qt 6 / C++17 (`QLoggingCategory`, `qInstallMessageHandler`, `qFormatLogMessage`, `QProcess`), Go 1.22 (`log/slog`, custom handler, `runtime.CallersFrames`), CMake/Ninja/CTest, Python 3 static contract check. No new third-party dependencies.

---

## Scope Check

This plan implements the logging standard only.

Included:
- Qt `miles.*` custom formatting: `HH:MM:SS.mmm LEVEL [MILES.X] message key=value [File.cpp:line]`.
- Qt optional `MILES_LOG_FILE`: truncate at app startup, then write through one file handle; file lines include date.
- Qt leaves non-`miles.*` categories in Qt default formatting and optionally writes them to the same file.
- Go `internal/mileslog` custom `slog.Handler` with category, source extraction from `record.PC`, env-driven level, and optional file output.
- Go replacement of `log.Printf`, `debugf`, `SetDebugLogging`, and `MILES_DEBUG_CHAT`.
- Payload safety: no API key ever logged; user/model raw text only when `MILES_LOG_PAYLOADS=1`.
- ChatController sidecar process logging cleanup: forward sidecar stderr instead of re-logging it through `miles.chat`.
- Static contract check and smoke tests for both logging implementations.

Excluded:
- Log rotation, compression, retention policy.
- UI log viewer.
- Remote logging / telemetry / Langfuse.
- Changing provider behavior or chat protocol.
- Broad refactors outside logging callsites.

## Assumptions

- Execution starts from `/Users/tian/projects/my-projects/MilesEdgeworth`.
- The current branch may already contain documentation-only changes. Commit those before implementation or intentionally carry them into the first docs commit; do not mix user-local `.vscode/` changes unless explicitly requested.
- `docs/v2/设计方案/日志规范设计.md` is the source of truth for formatting, environment variables, payload policy, and category names.
- Current build already includes Phase 2.3.1 changes: `ChatController`, `PetRuntime`, and `ExpressionMappingResolver` use `Q_LOGGING_CATEGORY(..., QtInfoMsg)`.
- Go sidecar currently uses `log.Printf`, `MILES_DEBUG_CHAT`, `openai.SetDebugLogging`, and `debugf`; this plan removes those.

## Required Reading

Before implementing, read:
- `docs/v2/设计方案/日志规范设计.md` all sections.
- `apps/desktop/src/main.cpp` for app startup order.
- `apps/desktop/src/chat/ChatController.cpp` constructor and `launchSidecarProcess()`.
- `apps/agent-core/cmd/miles-agent/main.go`.
- `apps/agent-core/internal/chat/openai/provider.go`.
- Existing tests: `apps/agent-core/internal/chat/openai/provider_test.go`, `apps/desktop/tests/chat_controller_smoke.cpp`.

## File Structure

Create:
- `apps/agent-core/internal/mileslog/mileslog.go`
  - Self-initializing Go logging package. Exports `New(category string) *slog.Logger` and `PayloadLoggingEnabled() bool`.
- `apps/agent-core/internal/mileslog/mileslog_test.go`
  - Unit tests for format, category, source extraction, level filtering, payload flag parsing.
- `apps/desktop/src/logging/MilesLogHandler.h`
  - Qt logging install function and testable formatting helpers.
- `apps/desktop/src/logging/MilesLogHandler.cpp`
  - Custom message handler, optional file sink, `miles.*` formatting, fallback default formatting for non-project categories.
- `apps/desktop/tests/logging_formatter_smoke.cpp`
  - C++ smoke test for category detection and formatting helpers.
- `tests/check_logging_standard.py`
  - Static contract check for the logging standard.

Modify:
- `CMakeLists.txt`
  - Register `check_logging_standard`.
- `apps/desktop/CMakeLists.txt`
  - Add logging module sources to `DESKTOP_SOURCES`; add `LoggingFormatterSmoke` test target.
- `apps/desktop/src/main.cpp`
  - Include `logging/MilesLogHandler.h`; call `MilesLogHandler::install()` before creating noisy subsystems.
- `apps/desktop/src/chat/ChatController.cpp`
  - Remove `logProcessOutput`, `readyReadStandardError`, `readyReadStandardOutput`; set `ForwardedErrorChannel`; forward `MILES_LOG_*` env vars; update launch log keys.
- `apps/desktop/src/pet/PetRuntime.cpp`
  - Normalize existing `miles.pet.runtime` / `miles.pet.expression` logs to no-quote `key=value` style where needed.
- `apps/desktop/src/pet/selection/ExpressionMappingResolver.cpp`
  - Normalize existing expression logs to no-quote `key=value` style where needed.
- `apps/agent-core/cmd/miles-agent/main.go`
  - Replace `log.Printf` / `log.Fatalf`; remove `MILES_DEBUG_CHAT` setup; use `mileslog.New("MILES.SIDECAR")`.
- `apps/agent-core/internal/api/server.go`
  - Add `mileslog.New("MILES.SIDECAR")` for request-level warnings/errors if needed by implementation.
- `apps/agent-core/internal/chat/openai/provider.go`
  - Replace `debugf` / `SetDebugLogging` with `logger.Debug`; add guarded payload logs.
- `tests/check_phase_2_1_provider.py`
  - Replace `MILES_DEBUG_CHAT` expectations if any exist.
- `docs/v2/阶段记录/Phase 2.3.1 动画-文字同步.md`
  - No required change unless final verification notes discover a logging-related caveat worth recording.

Not modified:
- `docs/v2/设计方案/日志规范设计.md` unless implementation reveals an actual design bug.
- QML UI.
- Provider request/response protocol.
- Secret storage behavior.

---

## Task 0: Preflight and Documentation Baseline

**Files:**
- Read-only unless committing existing docs.

- [ ] **Step 1: Check working tree**

Run:

```bash
git status --short --branch
```

Expected: current branch is the active Phase 2 branch. Existing dirty docs are acceptable only if they are intentional. `.vscode/` should remain untracked unless the user explicitly asks to commit it.

- [ ] **Step 2: Commit existing documentation changes if still dirty**

If `docs/v2/设计方案/日志规范设计.md`, `docs/v2/文档索引.md`, or `docs/v2/阶段记录/Phase 2.3.1 动画-文字同步.md` are dirty from the design-review work, commit them before implementation:

```bash
git add \
  "docs/v2/设计方案/日志规范设计.md" \
  "docs/v2/文档索引.md" \
  "docs/v2/阶段记录/Phase 2.3.1 动画-文字同步.md"
git commit -m "docs: 完善日志规范设计"
```

Expected: a docs-only commit. Do not stage `.vscode/`.

- [ ] **Step 3: Verify baseline tests**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
ctest --test-dir build -R "check_phase_2_1_provider|check_phase_2_2_settings|check_phase_2_3_1_animation_sync|chat_controller_smoke|pet_runtime_smoke" --output-on-failure
(cd apps/agent-core && go test ./...)
```

Expected: all pass before logging work starts.

- [ ] **Step 4: No implementation commit**

Task 0 changes nothing except the optional docs-baseline commit.

---

## Task 1: Static Logging Contract Scaffold

**Files:**
- Create: `tests/check_logging_standard.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing contract test**

Create `tests/check_logging_standard.py`:

```python
#!/usr/bin/env python3
"""Check the unified logging-standard implementation."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    file_path = ROOT / path
    if not file_path.exists():
        raise AssertionError(f"missing file: {path}")
    return file_path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    mileslog = read("apps/agent-core/internal/mileslog/mileslog.go")
    mileslog_test = read("apps/agent-core/internal/mileslog/mileslog_test.go")
    main_go = read("apps/agent-core/cmd/miles-agent/main.go")
    provider_go = read("apps/agent-core/internal/chat/openai/provider.go")
    qt_handler_h = read("apps/desktop/src/logging/MilesLogHandler.h")
    qt_handler_cpp = read("apps/desktop/src/logging/MilesLogHandler.cpp")
    qt_handler_smoke = read("apps/desktop/tests/logging_formatter_smoke.cpp")
    desktop_main = read("apps/desktop/src/main.cpp")
    chat_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")

    require("package mileslog" in mileslog, "Go mileslog package must exist")
    require("func New(category string) *slog.Logger" in mileslog,
            "mileslog.New(category) must be exported")
    require("func PayloadLoggingEnabled() bool" in mileslog,
            "mileslog must expose PayloadLoggingEnabled")
    require("type handler struct" in mileslog and "slog.Handler" in mileslog,
            "mileslog must implement a custom slog.Handler")
    require("runtime.CallersFrames" in mileslog and "record.PC" in mileslog,
            "mileslog must extract source from record.PC")
    require("MILES_LOG_LEVEL" in mileslog and "MILES_LOG_FILE" in mileslog
            and "MILES_LOG_PAYLOADS" in mileslog,
            "mileslog must read all logging env vars")
    require("WithGroup" not in mileslog,
            "mileslog must not use slog.WithGroup for category")
    require("category" in mileslog and "MILES." in mileslog_test,
            "mileslog tests must cover category formatting")

    require("MILES_DEBUG_CHAT" not in main_go + provider_go,
            "MILES_DEBUG_CHAT must be removed")
    require("SetDebugLogging" not in provider_go,
            "openai.SetDebugLogging must be removed")
    require("debugf(" not in provider_go,
            "provider debugf helper must be removed")
    require("log.Printf" not in main_go + provider_go,
            "sidecar must not use log.Printf")
    require("log.Fatalf" not in main_go + provider_go,
            "sidecar must not use log.Fatalf")
    require("mileslog.New(\"MILES.SIDECAR\")" in main_go,
            "main.go must use MILES.SIDECAR logger")
    require("mileslog.New(\"MILES.CHAT.PROVIDER\")" in provider_go,
            "provider.go must use MILES.CHAT.PROVIDER logger")

    require("namespace MilesLogHandler" in qt_handler_h,
            "Qt logging handler namespace must be declared")
    require("void install()" in qt_handler_h,
            "Qt logging handler must expose install()")
    require("qInstallMessageHandler" in qt_handler_cpp,
            "Qt logging handler must install a message handler")
    require("qFormatLogMessage" in qt_handler_cpp,
            "Qt logging handler must preserve default formatting for non-miles categories")
    require("MILES_LOG_FILE" in qt_handler_cpp,
            "Qt logging handler must support MILES_LOG_FILE")
    require("miles.*" in qt_handler_cpp or "miles." in qt_handler_cpp,
            "Qt logging handler must special-case miles.* categories")
    require("MilesLogHandler::install()" in desktop_main,
            "main.cpp must install the Qt logging handler")
    require("LoggingFormatterSmoke" in desktop_cmake and "logging_formatter_smoke" in desktop_cmake,
            "Qt logging formatter smoke test must be registered")
    require("check_logging_standard" in root_cmake,
            "root CMake must register check_logging_standard")

    require("ForwardedErrorChannel" in chat_cpp,
            "ChatController must forward sidecar stderr")
    require("readyReadStandardError" not in chat_cpp,
            "ChatController must not re-log sidecar stderr")
    require("logProcessOutput" not in chat_cpp,
            "ChatController logProcessOutput helper must be removed")
    require("MILES_LOG_PAYLOADS" in chat_cpp,
            "ChatController must forward MILES_LOG_PAYLOADS to sidecar")

    print("logging standard contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Register the contract test**

In root `CMakeLists.txt`, near the existing Phase 2 contract tests, add:

```cmake
        # Logging standard check: keeps Qt + Go logging categories, env vars,
        # payload safety, and sidecar stderr forwarding aligned with docs/v2/设计方案/日志规范设计.md.
        add_test(
            NAME check_logging_standard
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_logging_standard.py
        )
```

- [ ] **Step 3: Verify the contract fails for missing implementation**

Run:

```bash
python3 tests/check_logging_standard.py
```

Expected: FAIL with `missing file: apps/agent-core/internal/mileslog/mileslog.go`.

- [ ] **Step 4: Commit**

```bash
git add tests/check_logging_standard.py CMakeLists.txt
git commit -m "test: 增加日志规范契约检查"
```

---

## Task 2: Go `mileslog` Package

**Files:**
- Create: `apps/agent-core/internal/mileslog/mileslog.go`
- Create: `apps/agent-core/internal/mileslog/mileslog_test.go`

- [ ] **Step 1: Write failing Go tests**

Create `apps/agent-core/internal/mileslog/mileslog_test.go`:

```go
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
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
cd apps/agent-core
go test ./internal/mileslog
```

Expected: FAIL because package files do not exist or `newHandler` is undefined.

- [ ] **Step 3: Implement `mileslog`**

Create `apps/agent-core/internal/mileslog/mileslog.go`:

```go
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

func (h *handler) WithGroup(_ string) slog.Handler {
	return h
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
```

- [ ] **Step 4: Run Go tests**

Run:

```bash
cd apps/agent-core
go test ./internal/mileslog
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/agent-core/internal/mileslog/mileslog.go apps/agent-core/internal/mileslog/mileslog_test.go
git commit -m "feat: 增加 sidecar 日志封装"
```

---

## Task 3: Migrate Sidecar Logging

**Files:**
- Modify: `apps/agent-core/cmd/miles-agent/main.go`
- Modify: `apps/agent-core/internal/chat/openai/provider.go`
- Modify: `apps/agent-core/internal/chat/openai/provider_test.go`
- Modify: `tests/check_phase_2_1_provider.py` if it references `MILES_DEBUG_CHAT`

- [ ] **Step 1: Write failing migration assertions**

Append to `apps/agent-core/internal/chat/openai/provider_test.go`:

```go
func TestPayloadLoggingFlagDoesNotAffectStreamEvents(t *testing.T) {
	t.Setenv("MILES_LOG_PAYLOADS", "1")
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/event-stream")
		fmt.Fprint(w, "data: {\"choices\":[{\"delta\":{\"content\":\"[EXPR:polite]secret text\"}}]}\n\n")
		fmt.Fprint(w, "data: [DONE]\n\n")
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "any", 0.7, 128)
	events, err := p.StreamReply(context.Background(), chat.Request{
		Message:     "hi",
		Expressions: []chat.ExpressionInfo{{ID: "polite"}},
	})
	if err != nil {
		t.Fatalf("StreamReply: %v", err)
	}
	seenText := false
	for event := range events {
		if event.Type == "TEXT_MESSAGE_CONTENT" && event.Delta == "secret text" {
			seenText = true
		}
	}
	if !seenText {
		t.Fatal("payload logging flag must not change stream parsing")
	}
}
```

- [ ] **Step 2: Run tests to verify compile failure**

Run:

```bash
cd apps/agent-core
go test ./...
```

Expected: currently may still PASS because the test is behavioral. The real migration is enforced by `python3 tests/check_logging_standard.py`, which still fails on `MILES_DEBUG_CHAT`, `debugf`, and `log.Printf`.

- [ ] **Step 3: Migrate `main.go`**

In `apps/agent-core/cmd/miles-agent/main.go`:

Remove imports:

```go
	"log"
```

Add import:

```go
	"milesedgeworth/agent-core/internal/mileslog"
```

Add package-level logger after imports:

```go
var logger = mileslog.New("MILES.SIDECAR")
```

Delete this block:

```go
	debugChat := os.Getenv("MILES_DEBUG_CHAT") != ""
	openai.SetDebugLogging(debugChat)
	if debugChat {
		log.Printf("debug chat diagnostics enabled")
	}
```

Replace logging calls:

```go
logger.Info("provider selected", "provider", "openai-compatible", "model", cfg.Model)
logger.Info("provider selected", "provider", "mock-fallback")
logger.Info("server listening", "addr", *addr)
logger.Info("shutdown requested", "signal", sig.String())
logger.Error("shutdown failed", "error", err)
logger.Error("server failed", "error", err)
```

For fatal paths, log then exit:

```go
if err := server.Shutdown(ctx); err != nil {
	logger.Error("shutdown failed", "error", err)
	os.Exit(1)
}
```

and:

```go
if err != nil && err != http.ErrServerClosed {
	logger.Error("server failed", "error", err)
	os.Exit(1)
}
```

- [ ] **Step 4: Migrate `provider.go` logger**

In `apps/agent-core/internal/chat/openai/provider.go`:

Remove import:

```go
	"log"
```

Add import:

```go
	"milesedgeworth/agent-core/internal/mileslog"
```

Delete:

```go
var debugLoggingEnabled bool

func SetDebugLogging(enabled bool) {
	debugLoggingEnabled = enabled
}

func debugf(format string, args ...any) {
	if debugLoggingEnabled {
		log.Printf("debug chat.openai: "+format, args...)
	}
}
```

Add package-level logger:

```go
var logger = mileslog.New("MILES.CHAT.PROVIDER")
```

Replace debug calls with structured calls:

```go
logger.Debug("request prepared",
	"model", p.model,
	"endpoint", sanitizeEndpoint(p.baseURL),
	"expressions", len(req.Expressions),
	"messageLen", len([]rune(req.Message)))
```

```go
logger.Debug("stream started", "knownTags", strings.Join(knownTags, ","))
```

```go
logger.Debug("fallback expression inserted",
	"expression", "neutral",
	"textLen", len([]rune(text)))
```

```go
logger.Debug("text chunk parsed", "len", len([]rune(text)))
```

```go
logger.Debug("expression tag parsed", "tag", tag)
```

```go
logger.Debug("provider delta received", "len", len([]rune(choice.Delta.Content)))
if mileslog.PayloadLoggingEnabled() {
	logger.Debug("provider delta payload", "text", choice.Delta.Content)
}
```

```go
logger.Debug("stream finished")
```

When skipping malformed chunks, add a warning:

```go
if err := json.Unmarshal([]byte(payload), &chunk); err != nil {
	logger.Warn("malformed provider chunk skipped", "error", err)
	if mileslog.PayloadLoggingEnabled() {
		logger.Debug("malformed provider payload", "payload", payload)
	}
	continue
}
```

- [ ] **Step 5: Update stale contract references**

Run:

```bash
rg -n "MILES_DEBUG_CHAT|SetDebugLogging|debugf|log\\.Printf|log\\.Fatalf" apps/agent-core tests
```

Expected after edits: no matches except historical documentation if any. If `tests/check_phase_2_1_provider.py` requires `MILES_DEBUG_CHAT`, update it to require `MILES_LOG_LEVEL` / `mileslog.New`.

- [ ] **Step 6: Verify Go**

Run:

```bash
cd apps/agent-core
go test ./...
```

Expected: PASS.

- [ ] **Step 7: Verify contract progress**

Run:

```bash
python3 tests/check_logging_standard.py
```

Expected: still FAIL because Qt logging handler is missing, but no longer fails on Go `MILES_DEBUG_CHAT`, `debugf`, or `log.Printf`.

- [ ] **Step 8: Commit**

```bash
git add apps/agent-core/cmd/miles-agent/main.go \
  apps/agent-core/internal/chat/openai/provider.go \
  apps/agent-core/internal/chat/openai/provider_test.go \
  tests/check_phase_2_1_provider.py
git commit -m "feat: 迁移 sidecar 日志"
```

If `tests/check_phase_2_1_provider.py` was not changed, omit it from `git add`.

---

## Task 4: Qt Logging Handler and Formatter Smoke Test

**Files:**
- Create: `apps/desktop/src/logging/MilesLogHandler.h`
- Create: `apps/desktop/src/logging/MilesLogHandler.cpp`
- Create: `apps/desktop/tests/logging_formatter_smoke.cpp`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/src/main.cpp`

- [ ] **Step 1: Write failing Qt smoke test**

Create `apps/desktop/tests/logging_formatter_smoke.cpp`:

```cpp
#include "logging/MilesLogHandler.h"

#include <QCoreApplication>
#include <QMessageLogContext>
#include <QString>

#include <iostream>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    require(MilesLogHandler::isMilesCategory("miles.chat"),
            "miles.chat should be treated as project category");
    require(!MilesLogHandler::isMilesCategory("qt.qml.binding"),
            "qt.qml.binding should not be treated as project category");

    QMessageLogContext context("ChatController.cpp", 302, "sendMessage", "miles.chat");
    const QString consoleLine = MilesLogHandler::formatMilesMessage(
        QtInfoMsg,
        context,
        QStringLiteral("send chat request messageLen=12"),
        false);
    require(consoleLine.contains(QStringLiteral("INFO  [MILES.CHAT]")),
            "console line should contain padded INFO level and upper category");
    require(consoleLine.contains(QStringLiteral("send chat request messageLen=12")),
            "console line should contain message");
    require(consoleLine.endsWith(QStringLiteral("[ChatController.cpp:302]")),
            "console line should end with source");
    require(!consoleLine.left(10).contains('-'),
            "console line should omit date");

    const QString fileLine = MilesLogHandler::formatMilesMessage(
        QtDebugMsg,
        context,
        QStringLiteral("stream event type=CUSTOM"),
        true);
    require(fileLine.contains(QStringLiteral("DEBUG [MILES.CHAT]")),
            "file line should contain DEBUG level and category");
    require(fileLine.left(10).contains('-'),
            "file line should include date");

    std::cout << "logging formatter smoke checks passed.\n";
    return 0;
}
```

- [ ] **Step 2: Register test and sources**

In `apps/desktop/CMakeLists.txt`, add to `DESKTOP_SOURCES` near `src/main.cpp`:

```cmake
    src/logging/MilesLogHandler.cpp
    src/logging/MilesLogHandler.h
```

Inside `if(BUILD_TESTING)`, add:

```cmake
    add_executable(LoggingFormatterSmoke
        tests/logging_formatter_smoke.cpp
        src/logging/MilesLogHandler.cpp
        src/logging/MilesLogHandler.h
    )

    target_include_directories(LoggingFormatterSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LoggingFormatterSmoke
        PRIVATE
            Qt6::Core
    )

    add_test(NAME logging_formatter_smoke COMMAND LoggingFormatterSmoke)
```

- [ ] **Step 3: Run test to verify failure**

Run:

```bash
cmake --build build --target LoggingFormatterSmoke
```

Expected: FAIL because `logging/MilesLogHandler.h` does not exist.

- [ ] **Step 4: Add `MilesLogHandler.h`**

Create `apps/desktop/src/logging/MilesLogHandler.h`:

```cpp
#pragma once

#include <QtGlobal>

class QMessageLogContext;
class QString;

namespace MilesLogHandler {

void install();
bool isMilesCategory(const char *category);
QString formatMilesMessage(QtMsgType type,
                           const QMessageLogContext &context,
                           const QString &message,
                           bool includeDate);

} // namespace MilesLogHandler
```

- [ ] **Step 5: Add `MilesLogHandler.cpp`**

Create `apps/desktop/src/logging/MilesLogHandler.cpp`:

```cpp
#include "logging/MilesLogHandler.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMessageLogContext>
#include <QMutex>
#include <QTextStream>

#include <cstdio>
#include <memory>

namespace {

QtMessageHandler previousHandler = nullptr;
QMutex logMutex;
std::unique_ptr<QFile> logFile;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO ");
    case QtWarningMsg:
        return QStringLiteral("WARN ");
    case QtCriticalMsg:
    case QtFatalMsg:
        return QStringLiteral("ERROR");
    }
    return QStringLiteral("INFO ");
}

QString sourceName(const QMessageLogContext &context)
{
    if (context.file == nullptr || context.line <= 0) {
        return QString();
    }
    return QFileInfo(QString::fromUtf8(context.file)).fileName()
        + QStringLiteral(":")
        + QString::number(context.line);
}

void writeStderr(const QString &line)
{
    const QByteArray bytes = line.toLocal8Bit();
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stderr);
    std::fwrite("\n", 1, 1, stderr);
    std::fflush(stderr);
}

void writeFileLine(const QString &line)
{
    if (logFile == nullptr || !logFile->isOpen()) {
        return;
    }
    QTextStream stream(logFile.get());
    stream << line << '\n';
    stream.flush();
}

void handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QMutexLocker locker(&logMutex);
    if (MilesLogHandler::isMilesCategory(context.category)) {
        writeStderr(MilesLogHandler::formatMilesMessage(type, context, message, false));
        writeFileLine(MilesLogHandler::formatMilesMessage(type, context, message, true));
        return;
    }

    if (previousHandler != nullptr) {
        previousHandler(type, context, message);
    } else {
        writeStderr(qFormatLogMessage(type, context, message));
    }

    if (logFile != nullptr && logFile->isOpen()) {
        writeFileLine(qFormatLogMessage(type, context, message));
    }
}

} // namespace

namespace MilesLogHandler {

void install()
{
    const QByteArray path = qgetenv("MILES_LOG_FILE");
    if (!path.trimmed().isEmpty()) {
        logFile = std::make_unique<QFile>(QString::fromLocal8Bit(path));
        logFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    }
    previousHandler = qInstallMessageHandler(handleMessage);
}

bool isMilesCategory(const char *category)
{
    if (category == nullptr) {
        return false;
    }
    return QByteArray(category).startsWith("miles.");
}

QString formatMilesMessage(QtMsgType type,
                           const QMessageLogContext &context,
                           const QString &message,
                           bool includeDate)
{
    const QString timestamp = QDateTime::currentDateTime().toString(
        includeDate ? QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz") : QStringLiteral("HH:mm:ss.zzz"));
    const QString category = QString::fromUtf8(context.category == nullptr ? "miles.app" : context.category)
        .toUpper();
    QString line = timestamp
        + QStringLiteral(" ")
        + levelName(type)
        + QStringLiteral(" [")
        + category
        + QStringLiteral("] ")
        + message;
    const QString source = sourceName(context);
    if (!source.isEmpty()) {
        line += QStringLiteral(" [") + source + QStringLiteral("]");
    }
    return line;
}

} // namespace MilesLogHandler
```

- [ ] **Step 6: Install handler in app startup**

In `apps/desktop/src/main.cpp`, add include:

```cpp
#include "logging/MilesLogHandler.h"
```

At the top of `main`, before `QApplication app(argc, argv);`, add:

```cpp
    MilesLogHandler::install();
```

- [ ] **Step 7: Run Qt logging smoke**

Run:

```bash
cmake --build build --target LoggingFormatterSmoke
build/apps/desktop/LoggingFormatterSmoke
```

Expected:

```text
logging formatter smoke checks passed.
```

- [ ] **Step 8: Commit**

```bash
git add apps/desktop/src/logging/MilesLogHandler.h \
  apps/desktop/src/logging/MilesLogHandler.cpp \
  apps/desktop/tests/logging_formatter_smoke.cpp \
  apps/desktop/CMakeLists.txt \
  apps/desktop/src/main.cpp
git commit -m "feat: 增加 Qt 日志处理器"
```

---

## Task 5: Sidecar Process Logging Plumbing in ChatController

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Test: `tests/check_logging_standard.py`

- [ ] **Step 1: Write failing contract additions if missing**

Confirm `tests/check_logging_standard.py` already checks:

```python
require("ForwardedErrorChannel" in chat_cpp,
        "ChatController must forward sidecar stderr")
require("readyReadStandardError" not in chat_cpp,
        "ChatController must not re-log sidecar stderr")
require("logProcessOutput" not in chat_cpp,
        "ChatController logProcessOutput helper must be removed")
require("MILES_LOG_PAYLOADS" in chat_cpp,
        "ChatController must forward MILES_LOG_PAYLOADS to sidecar")
```

Run:

```bash
python3 tests/check_logging_standard.py
```

Expected: FAIL on `readyReadStandardError`, `logProcessOutput`, or missing `ForwardedErrorChannel`.

- [ ] **Step 2: Remove sidecar stderr/stdout re-logging helper**

In `apps/desktop/src/chat/ChatController.cpp`, delete the full `logProcessOutput` helper:

```cpp
void logProcessOutput(const char *label, const QByteArray &bytes)
{
    const QList<QByteArray> lines = bytes.split('\n');
    for (QByteArray line : lines) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (!line.trimmed().isEmpty()) {
            qCDebug(chatLog).noquote() << label << QString::fromLocal8Bit(line);
        }
    }
}
```

Delete these constructor connections:

```cpp
    connect(&m_sidecarProcess, &QProcess::readyReadStandardError, this, [this]() {
        logProcessOutput("sidecar stderr", m_sidecarProcess.readAllStandardError());
    });
    connect(&m_sidecarProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        logProcessOutput("sidecar stdout", m_sidecarProcess.readAllStandardOutput());
    });
```

- [ ] **Step 3: Forward sidecar stderr**

In the `ChatController` constructor, before `connect(&m_sidecarProcess, &QProcess::started, ...)`, add:

```cpp
    m_sidecarProcess.setProcessChannelMode(QProcess::ForwardedErrorChannel);
```

- [ ] **Step 4: Make log env forwarding explicit**

In `launchSidecarProcess()`, immediately after:

```cpp
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
```

add:

```cpp
    const QProcessEnvironment systemEnv = QProcessEnvironment::systemEnvironment();
    const QStringList logEnvNames{
        QStringLiteral("MILES_LOG_FILE"),
        QStringLiteral("MILES_LOG_LEVEL"),
        QStringLiteral("MILES_LOG_PAYLOADS"),
    };
    for (const QString &name : logEnvNames) {
        if (systemEnv.contains(name)) {
            env.insert(name, systemEnv.value(name));
        }
    }
```

This is redundant with `systemEnvironment()` today, but makes the logging contract explicit and robust if `env` is later constructed from scratch.

- [ ] **Step 5: Update launch debug keys**

Replace the old `debug=` field:

```cpp
                     << "debug=" << env.contains(QStringLiteral("MILES_DEBUG_CHAT"));
```

with no-quote key/value fields:

```cpp
                     << QStringLiteral("logLevel=%1").arg(env.value(QStringLiteral("MILES_LOG_LEVEL"), QStringLiteral("info")))
                     << QStringLiteral("logFileSet=%1").arg(env.contains(QStringLiteral("MILES_LOG_FILE")))
                     << QStringLiteral("payloads=%1").arg(env.value(QStringLiteral("MILES_LOG_PAYLOADS")) == QStringLiteral("1"));
```

Also convert the whole launch log to `.noquote()`:

```cpp
    qCDebug(chatLog).noquote() << "launch sidecar"
                               << QStringLiteral("path=%1").arg(executablePath)
                               << QStringLiteral("baseUrlSet=%1").arg(env.contains(QStringLiteral("MILES_PROVIDER_BASE_URL")))
                               << QStringLiteral("apiKeySet=%1").arg(env.contains(QStringLiteral("MILES_PROVIDER_API_KEY")))
                               << QStringLiteral("model=%1").arg(env.value(QStringLiteral("MILES_PROVIDER_MODEL")))
                               << QStringLiteral("logLevel=%1").arg(env.value(QStringLiteral("MILES_LOG_LEVEL"), QStringLiteral("info")))
                               << QStringLiteral("logFileSet=%1").arg(env.contains(QStringLiteral("MILES_LOG_FILE")))
                               << QStringLiteral("payloads=%1").arg(env.value(QStringLiteral("MILES_LOG_PAYLOADS")) == QStringLiteral("1"));
```

- [ ] **Step 6: Verify contract**

Run:

```bash
python3 tests/check_logging_standard.py
```

Expected: may still FAIL on remaining Qt log normalization, but must no longer fail on sidecar forwarding.

- [ ] **Step 7: Build ChatControllerSmoke**

Run:

```bash
cmake --build build --target ChatControllerSmoke
build/apps/desktop/ChatControllerSmoke
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add apps/desktop/src/chat/ChatController.cpp tests/check_logging_standard.py
git commit -m "feat: 转发 sidecar 日志输出"
```

If `tests/check_logging_standard.py` did not change in this task, omit it from `git add`.

---

## Task 6: Normalize Project Log Calls and Payload Safety

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/src/pet/selection/ExpressionMappingResolver.cpp`
- Modify: `apps/agent-core/internal/chat/openai/provider.go`

- [ ] **Step 1: Add no-quote formatting rule to contract**

Extend `tests/check_logging_standard.py` with:

```python
    require(".noquote()" in chat_cpp,
            "ChatController should use noquote key=value debug logs")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    expression_cpp = read("apps/desktop/src/pet/selection/ExpressionMappingResolver.cpp")
    require(".noquote()" in runtime_cpp,
            "PetRuntime should use noquote key=value debug logs")
    require(".noquote()" in expression_cpp,
            "ExpressionMappingResolver should use noquote key=value debug logs")
    require("PayloadLoggingEnabled" in provider_go,
            "provider must guard raw payload logs behind MILES_LOG_PAYLOADS")
```

Run:

```bash
python3 tests/check_logging_standard.py
```

Expected: FAIL until the Qt callsites are normalized.

- [ ] **Step 2: Normalize ChatController logs**

For `ChatController.cpp`, convert current `qCDebug(chatLog) <<` calls to `.noquote()` and `QStringLiteral("key=%1").arg(...)` style. Examples:

```cpp
qCDebug(chatLog).noquote() << "sidecar process started"
                           << QStringLiteral("pid=%1").arg(m_sidecarProcess.processId())
                           << QStringLiteral("program=%1").arg(m_sidecarProcess.program());
```

```cpp
qCDebug(chatLog).noquote() << "sidecar health"
                           << QStringLiteral("healthy=%1").arg(healthy)
                           << QStringLiteral("pid=%1").arg(healthPid)
                           << QStringLiteral("ownedPid=%1").arg(ownedPid)
                           << QStringLiteral("provider=%1").arg(health.value(QStringLiteral("provider")).toString())
                           << QStringLiteral("error=%1").arg(reply->errorString());
```

```cpp
qCDebug(chatLog).noquote() << "stream event"
                           << QStringLiteral("type=%1").arg(event.type)
                           << QStringLiteral("name=%1").arg(event.name)
                           << QStringLiteral("state=%1").arg(event.value.value(QStringLiteral("state")).toString())
                           << QStringLiteral("expression=%1").arg(event.value.value(QStringLiteral("expression")).toString())
                           << QStringLiteral("deltaLen=%1").arg(event.delta.size());
```

Do not log user message text or model text. Keep `messageLen` and `deltaLen` only.

- [ ] **Step 3: Normalize PetRuntime logs**

For `PetRuntime.cpp`, convert logs to `.noquote()`. Examples:

```cpp
qCDebug(petRuntimeLog).noquote() << "execute action request"
                                 << QStringLiteral("kind=%1").arg(static_cast<int>(request.kind))
                                 << QStringLiteral("target=%1").arg(request.target)
                                 << QStringLiteral("state=%1").arg(request.state)
                                 << QStringLiteral("interruptHint=%1").arg(static_cast<int>(request.interruptHint));
```

```cpp
qCDebug(petRuntimeLog).noquote() << "set current phase"
                                 << QStringLiteral("action=%1").arg(m_currentActionId)
                                 << QStringLiteral("phase=%1").arg(m_currentPhaseId)
                                 << QStringLiteral("loopMode=%1").arg(phase.loopMode)
                                 << QStringLiteral("url=%1").arg(phase.url.toString())
                                 << QStringLiteral("serial=%1").arg(m_playbackSerial)
                                 << QStringLiteral("replacing=%1").arg(replacing);
```

- [ ] **Step 4: Normalize ExpressionMappingResolver logs**

For `ExpressionMappingResolver.cpp`, convert logs to `.noquote()`. Example:

```cpp
qCDebug(petExpressionLog).noquote() << "resolve expression"
                                    << QStringLiteral("requested=%1").arg(expression)
                                    << QStringLiteral("resolved=%1").arg(resolvedExpression)
                                    << QStringLiteral("state=%1").arg(state)
                                    << QStringLiteral("candidates=%1").arg(candidates.size())
                                    << QStringLiteral("selection=%1").arg(selection);
```

- [ ] **Step 5: Keep payload logs guarded in Go**

In `provider.go`, raw provider deltas or malformed raw payloads must only appear inside:

```go
if mileslog.PayloadLoggingEnabled() {
	logger.Debug("provider delta payload", "text", choice.Delta.Content)
}
```

and:

```go
if mileslog.PayloadLoggingEnabled() {
	logger.Debug("malformed provider payload", "payload", payload)
}
```

Do not log `p.apiKey`, `Authorization`, or full request bodies.

- [ ] **Step 6: Verify static contract**

Run:

```bash
python3 tests/check_logging_standard.py
```

Expected: PASS if all earlier tasks are complete.

- [ ] **Step 7: Build and run smoke tests**

Run:

```bash
cmake --build build --target ChatControllerSmoke PetRuntimeSmoke LoggingFormatterSmoke
build/apps/desktop/ChatControllerSmoke
build/apps/desktop/PetRuntimeSmoke
build/apps/desktop/LoggingFormatterSmoke
```

Expected: all pass.

- [ ] **Step 8: Commit**

```bash
git add apps/desktop/src/chat/ChatController.cpp \
  apps/desktop/src/pet/PetRuntime.cpp \
  apps/desktop/src/pet/selection/ExpressionMappingResolver.cpp \
  apps/agent-core/internal/chat/openai/provider.go \
  tests/check_logging_standard.py
git commit -m "chore: 统一项目日志格式"
```

---

## Task 7: End-to-End Logging Verification

**Files:**
- Modify only if verification reveals a small contract/doc mismatch.

- [ ] **Step 1: Run all logging tests**

Run:

```bash
python3 tests/check_logging_standard.py
ctest --test-dir build -R "check_logging_standard|logging_formatter_smoke|chat_controller_smoke|pet_runtime_smoke" --output-on-failure
(cd apps/agent-core && go test ./...)
```

Expected: all pass.

- [ ] **Step 2: Build the desktop app**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop
```

Expected: build succeeds and `miles-agent` is copied into the app bundle.

- [ ] **Step 3: Run with debug logs to file**

Run:

```bash
rm -f /tmp/miles-debug.log
QT_LOGGING_RULES='miles.*.debug=true' \
MILES_LOG_LEVEL=debug \
MILES_LOG_FILE=/tmp/miles-debug.log \
MILES_LOG_PAYLOADS=0 \
build/apps/desktop/MilesEdgeworthDesktop.app/Contents/MacOS/MilesEdgeworthDesktop
```

Manual action: send one chat message, then quit the app.

Expected:
- Debug console shows lines like:

```text
17:12:20.123 DEBUG [MILES.CHAT] stream event type=CUSTOM ...
17:12:20.124 DEBUG [MILES.CHAT.PROVIDER] expression tag parsed tag=objection ...
```

- `/tmp/miles-debug.log` exists and contains full-date lines like:

```text
2026-05-22 17:12:20.123 DEBUG [MILES.CHAT] stream event type=CUSTOM ...
```

- `/tmp/miles-debug.log` does not contain API key values.
- With `MILES_LOG_PAYLOADS=0`, the file does not contain raw user message text or raw model text.

- [ ] **Step 4: Run payload opt-in smoke**

Run again with:

```bash
rm -f /tmp/miles-debug-payloads.log
QT_LOGGING_RULES='miles.*.debug=true' \
MILES_LOG_LEVEL=debug \
MILES_LOG_FILE=/tmp/miles-debug-payloads.log \
MILES_LOG_PAYLOADS=1 \
build/apps/desktop/MilesEdgeworthDesktop.app/Contents/MacOS/MilesEdgeworthDesktop
```

Manual action: send a harmless message like `payload smoke test`.

Expected:
- Go provider payload debug lines may include model delta text when using real provider.
- API key is still absent.
- This log file is local-only and must not be committed.

- [ ] **Step 5: Verify no stale env or helper names remain**

Run:

```bash
rg -n "MILES_DEBUG_CHAT|SetDebugLogging|debugf\\(|logProcessOutput|readyReadStandardError|log\\.Printf|log\\.Fatalf" apps tests
```

Expected: no matches in implementation or tests. If matches appear only in archived docs, leave them; if matches appear in active code/tests, fix them.

- [ ] **Step 6: Commit final verification docs if needed**

If no files changed during verification:

```bash
git status --short
```

Expected: clean except user-local `.vscode/` if still untracked.

If a small doc or contract adjustment was required:

```bash
git add \
  tests/check_logging_standard.py \
  "docs/v2/设计方案/日志规范设计.md" \
  "docs/v2/阶段记录/Phase 2.3.1 动画-文字同步.md"
git commit -m "docs: 补充日志规范落地说明"
```

---

## Final Verification Checklist

Run before PR or merge:

```bash
python3 tests/check_logging_standard.py
python3 tests/check_phase_2_1_provider.py
python3 tests/check_phase_2_2_settings.py
python3 tests/check_phase_2_3_1_animation_sync.py
(cd apps/agent-core && go test ./...)
cmake --build build --target MilesEdgeworthDesktop ChatControllerSmoke PetRuntimeSmoke LoggingFormatterSmoke
ctest --test-dir build -R "check_logging_standard|logging_formatter_smoke|chat_controller_smoke|pet_runtime_smoke|settings_service_smoke" --output-on-failure
```

Expected:
- All commands pass.
- `git status --short` is clean except intentionally ignored/untracked local files.
- No log file containing payloads is staged.

## Self-Review

Spec coverage:
- Categories: Task 1 contract + Tasks 2, 4, 6.
- Go logger category without `WithGroup`: Task 2.
- `MILES_LOG_LEVEL`, `MILES_LOG_FILE`, `MILES_LOG_PAYLOADS`: Tasks 2, 5, 7.
- Qt `miles.*` custom formatting and non-project fallback: Task 4.
- Sidecar stderr forwarding and no re-log helper: Task 5.
- Payload safety: Tasks 2, 3, 6, 7.
- Source file/line: Task 2 and Task 4.
- File output best-effort cross-process behavior: Task 2 and Task 4 implement the two-process sink; Task 7 verifies output.

Placeholder scan:
- No banned placeholder markers or vague “do later” steps.
- Every code-creating task includes concrete code or exact replacement snippets.
- All commands include expected result.

Type/name consistency:
- Go package: `internal/mileslog`, `mileslog.New`, `PayloadLoggingEnabled`, `newHandler`, `sink`.
- Qt namespace: `MilesLogHandler`, `install`, `isMilesCategory`, `formatMilesMessage`.
- Contract names match implementation names.
