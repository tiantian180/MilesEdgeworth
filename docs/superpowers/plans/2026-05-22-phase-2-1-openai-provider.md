# Phase 2.1 OpenAI-compatible Provider Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a real OpenAI-compatible Chat Completions provider to the Go sidecar and introduce the `[EXPR:x]` expression marker protocol end-to-end, while keeping the existing mock provider as a fallback when no API key is configured.

**Architecture:** A new `internal/chat/openai` package wraps Chat Completions with streaming SSE consumption. A new `internal/chat/expression` package parses `[EXPR:tag]` markers out of the model's token stream and emits separate `miles.pet.expression.requested` events vs `TEXT_MESSAGE_CONTENT` events. A new `internal/chat/config` package reads env vars; `main.go` chooses provider based on `Enabled()`. On the Qt side, `ChatController` ships the current skin's expression list in each request and gets a thin "hold buffer" placeholder where the full state machine (Phase 2.3) will land later.

**Tech Stack:** Go 1.22 standard library (`net/http`, `bufio`, `encoding/json`), `httptest` for provider unit tests, Qt 6 C++17 (`QNetworkAccessManager`, `QJsonObject`), CMake/Ninja/CTest, existing Python contract check style.

---

## Scope Check

This plan implements **Phase 2.1: OpenAI-compatible Provider** only.

Included:
- `internal/chat/config` env-var-based provider config.
- `internal/chat/expression` `[EXPR:tag]` token parser with cross-chunk buffering.
- `internal/chat/openai` provider adapter: builds system prompt with current skin's expressions, hits `/v1/chat/completions` streaming, transforms upstream SSE to our AG-UI events through the expression parser.
- `main.go` provider selection: real provider when config complete, otherwise mock fallback.
- `Request` struct gets explicit JSON tags and an `Expressions` field.
- Qt side: `ChatController` includes the current manifest's expressions in each chat request; new `m_holdBuffer` plumbing (immediate flush for now — full state machine is Phase 2.3).
- Server hardening: `MaxBytesReader` for request body, extract listen address into a shared constant.
- Go unit tests (config, parser, openai provider via httptest), updated Qt smoke test, Python contract check.

Excluded:
- Full ChatController state machine (`BUFFERING_FOR_START`/`STREAMING`/`GATED`/`WAITING_FOR_ANIMATION_END`) → Phase 2.3.
- Character rate limiter → Phase 2.3.
- Settings UI, Keychain / Credential Manager / Secret Service → Phase 2.2.
- Conversation history persistence → Phase 2.3.
- Phased animation (enter/loop/exit), `requestCleanFinishAndNotify` / `requestBoundaryAndNotify` interfaces → Phase 2.4.
- Multi-modal image input → Phase 2.6.

## Assumptions

- Execution starts from `/Users/tian/projects/my-projects/MilesEdgeworth` on `main`.
- Phase 2.0 has shipped: `apps/agent-core` exists with mock provider, Qt `ChatController`/`ChatStreamEventParser` exist, smoke tests pass.
- `go` 1.22+ is installed (used in Phase 2.0).
- The current skin (`miles-edgeworth`) declares `expressions: [neutral, objection, polite]` in its manifest.
- CI does not call real OpenAI-compatible endpoints; all provider tests use `httptest`.
- The reference design for marker protocol and event ordering is `docs/v2/设计方案/AI 聊天动画编排设计.md`. This plan implements the **§11 Phase 2.1** row of that document.

## File Structure

Create:
- `apps/agent-core/internal/chat/config/config.go`
  - `ProviderConfig` struct with `BaseURL/APIKey/Model/Temperature/MaxTokens`; `FromEnv()` reads `MILES_PROVIDER_*` env vars; `Enabled()` returns true when base URL + API key + model are all set.
- `apps/agent-core/internal/chat/config/config_test.go`
  - Table-driven tests for `FromEnv()` and `Enabled()` covering missing vars and parsing of temperature/max_tokens.
- `apps/agent-core/internal/chat/expression/parser.go`
  - Streaming `[EXPR:tag]` marker parser; `Feed(chunk string)` splits stream into `OnText`/`OnExpression` callbacks; `Flush()` emits any held tail as text; unknown tags fall back to a configurable default.
- `apps/agent-core/internal/chat/expression/parser_test.go`
  - Tests for pure text, single marker at start, marker in middle, cross-chunk marker, unknown tag fallback, half-marker at stream end, `[ABC` not-a-marker case.
- `apps/agent-core/internal/chat/openai/provider.go`
  - `Provider` implementing `chat.Provider`; `buildSystemPrompt(expressions)`; `StreamReply` POSTs to `/v1/chat/completions`, scans SSE response, feeds content through `expression.Parser`, emits AG-UI envelope events.
- `apps/agent-core/internal/chat/openai/provider_test.go`
  - `httptest`-based tests covering: request body shape, happy-path stream transformation, 4xx returns `RUN_ERROR`, malformed upstream JSON is skipped.
- `tests/check_phase_2_1_provider.py`
  - Static contract check for the new files and integration points.
- `docs/v2/阶段记录/Phase 2.1 OpenAI-compatible Provider.md`
  - Implementation record produced in the final task.

Modify:
- `apps/agent-core/internal/chat/provider.go`
  - Add JSON tags to `Request`; add `ExpressionInfo` struct and `Expressions []ExpressionInfo` field on `Request`.
- `apps/agent-core/internal/api/server.go`
  - Accept a provider label string for `/health`; add `http.MaxBytesReader` to chat handler; extract default listen address into a shared constant.
- `apps/agent-core/cmd/miles-agent/main.go`
  - Read `config.FromEnv()`; instantiate `openai.NewProvider` when enabled, otherwise mock; pass label to `api.NewServer`.
- `apps/desktop/src/chat/ChatController.h`
  - Add `m_holdBuffer` member and `flushHoldBuffer()` declaration.
- `apps/desktop/src/chat/ChatController.cpp`
  - Route `appendAssistantDelta` through `m_holdBuffer`/`flushHoldBuffer`; include current manifest expressions in `sendMessage` request body.
- `apps/desktop/tests/chat_controller_smoke.cpp`
  - Add assertions for hold-buffer round-trip and expressions list in serialized body (introspected via a stub `sendMessage` path or via a new helper).
- `CMakeLists.txt`
  - Register `check_phase_2_1_provider` ctest.
- `docs/v2/文档索引.md`
  - Add a link to the Phase 2.1 stage record.

Not modified (intentional):
- `apps/agent-core/internal/chat/mock_provider.go` — mock keeps emitting structured events directly (it's a test fixture, not a real provider).
- `apps/desktop/src/pet/PetRuntime.{h,cpp}` — `manifest()` accessor is already public; `ChatController` reads expressions through it.

## Task 0: Toolchain Preflight

**Files:**
- Read: `CMakeLists.txt`

- [ ] **Step 1: Verify branch and cleanliness**

Run:

```bash
git status --short --branch
```

Expected: `## main...origin/main` and a (possibly non-empty) list of unrelated user changes. Do not touch any unrelated changes.

- [ ] **Step 2: Verify Go toolchain**

Run:

```bash
go version
```

Expected: `go version go1.22` or newer.

- [ ] **Step 3: Verify Phase 2.0 baseline tests pass**

Run:

```bash
(cd apps/agent-core && go test ./...) && python3 tests/check_phase_2_0_ai_chat_mvp.py
```

Expected: all green, `phase 2.0 ai chat mvp contract ok`.

- [ ] **Step 4: No commit**

This task changes nothing.

## Task 1: Phase 2.1 Static Contract Scaffold

**Files:**
- Create: `tests/check_phase_2_1_provider.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the contract test**

Create `tests/check_phase_2_1_provider.py`:

```python
#!/usr/bin/env python3
"""Check the Phase 2.1 OpenAI-compatible provider contract."""

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
    config_go = read("apps/agent-core/internal/chat/config/config.go")
    config_test = read("apps/agent-core/internal/chat/config/config_test.go")
    parser_go = read("apps/agent-core/internal/chat/expression/parser.go")
    parser_test = read("apps/agent-core/internal/chat/expression/parser_test.go")
    openai_go = read("apps/agent-core/internal/chat/openai/provider.go")
    openai_test = read("apps/agent-core/internal/chat/openai/provider_test.go")
    main_go = read("apps/agent-core/cmd/miles-agent/main.go")
    server_go = read("apps/agent-core/internal/api/server.go")
    provider_go = read("apps/agent-core/internal/chat/provider.go")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    controller_smoke = read("apps/desktop/tests/chat_controller_smoke.cpp")
    root_cmake = read("CMakeLists.txt")
    index_doc = read("docs/v2/文档索引.md")
    phase_record = read("docs/v2/阶段记录/Phase 2.1 OpenAI-compatible Provider.md")

    require("MILES_PROVIDER_BASE_URL" in config_go, "config must read MILES_PROVIDER_BASE_URL")
    require("MILES_PROVIDER_API_KEY" in config_go, "config must read MILES_PROVIDER_API_KEY")
    require("MILES_PROVIDER_MODEL" in config_go, "config must read MILES_PROVIDER_MODEL")
    require("func (c ProviderConfig) Enabled()" in config_go, "config must expose Enabled receiver method")
    require("TestFromEnv" in config_test, "config tests must cover FromEnv")

    require("type Parser struct" in parser_go, "expression parser must be a struct type")
    require("Feed(chunk string)" in parser_go, "parser must expose Feed")
    require("Flush()" in parser_go, "parser must expose Flush")
    require("OnText" in parser_go and "OnExpression" in parser_go, "parser callbacks must be exposed")
    require("[EXPR:" in parser_go, "parser must reference the EXPR marker syntax")
    require("TestParserCrossChunk" in parser_test, "parser tests must cover cross-chunk markers")
    require("TestParserUnknownTag" in parser_test, "parser tests must cover unknown tags")

    require("/v1/chat/completions" in openai_go, "openai provider must POST to /v1/chat/completions")
    require("Bearer " in openai_go, "openai provider must send Bearer token")
    require("buildSystemPrompt" in openai_go, "openai provider must assemble a system prompt")
    require("expression.NewParser" in openai_go, "openai provider must use the expression parser")
    require("httptest.NewServer" in openai_test, "openai provider tests must use httptest")

    require("openai.NewProvider" in main_go, "main must construct openai provider when configured")
    require("mock-fallback" in main_go, "main must label provider as mock-fallback when config missing")

    require("http.MaxBytesReader" in server_go, "server must enforce max request body size")
    require("const DefaultListenAddr" in server_go or "DefaultListenAddr =" in server_go,
            "server must expose a shared default listen address constant")

    require('json:"conversationId"' in provider_go, "Request must use json tag conversationId")
    require('json:"message"' in provider_go, "Request must use json tag message")
    require('json:"expressions,omitempty"' in provider_go, "Request must carry expressions list")
    require("type ExpressionInfo struct" in provider_go, "ExpressionInfo struct must exist")

    require("m_holdBuffer" in controller_h, "ChatController must declare a hold buffer member")
    require("flushHoldBuffer" in controller_h, "ChatController must declare flushHoldBuffer")
    require("m_holdBuffer" in controller_cpp, "ChatController.cpp must use the hold buffer")
    require('"expressions"' in controller_cpp, "ChatController must include expressions field in request body")

    require("hold buffer" in controller_smoke.lower() or "m_holdBuffer" in controller_smoke,
            "smoke test must cover the hold buffer path")

    require("check_phase_2_1_provider" in root_cmake, "root CMake must register Phase 2.1 contract check")
    require("Phase 2.1" in phase_record, "phase 2.1 record must exist")
    require("Phase 2.1" in index_doc, "doc index must link Phase 2.1 record")

    print("phase 2.1 provider contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Run to verify it fails**

Run:

```bash
python3 tests/check_phase_2_1_provider.py
```

Expected: `AssertionError: missing file: apps/agent-core/internal/chat/config/config.go` (or similar — fails fast on first missing artifact).

- [ ] **Step 3: Register in root CMakeLists.txt**

Open `CMakeLists.txt` and find the existing `check_phase_2_0_ai_chat_mvp` registration. Add a parallel registration immediately after.

Find:

```cmake
add_test(NAME check_phase_2_0_ai_chat_mvp
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/check_phase_2_0_ai_chat_mvp.py
)
```

Append after that block:

```cmake
add_test(NAME check_phase_2_1_provider
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/check_phase_2_1_provider.py
)
```

If the exact `add_test` invocation differs (e.g. uses a different python variable), match the existing style verbatim and only change the test name + script path.

- [ ] **Step 4: Verify CTest sees it**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew >/dev/null 2>&1 && ctest --test-dir build -N -R check_phase_2_1_provider
```

Expected output contains `Test #N: check_phase_2_1_provider`.

- [ ] **Step 5: Commit**

```bash
git add tests/check_phase_2_1_provider.py CMakeLists.txt
git commit -m "test(phase-2-1): add provider contract scaffold (red)"
```

## Task 2: Provider Config from Env

**Files:**
- Create: `apps/agent-core/internal/chat/config/config.go`
- Create: `apps/agent-core/internal/chat/config/config_test.go`

- [ ] **Step 1: Write the failing test**

Create `apps/agent-core/internal/chat/config/config_test.go`:

```go
package config_test

import (
	"testing"

	"milesedgeworth/agent-core/internal/chat/config"
)

func TestFromEnv(t *testing.T) {
	t.Setenv("MILES_PROVIDER_BASE_URL", "https://api.example.com")
	t.Setenv("MILES_PROVIDER_API_KEY", "sk-test")
	t.Setenv("MILES_PROVIDER_MODEL", "gpt-test")
	t.Setenv("MILES_PROVIDER_TEMPERATURE", "0.3")
	t.Setenv("MILES_PROVIDER_MAX_TOKENS", "1024")

	cfg := config.FromEnv()

	if cfg.BaseURL != "https://api.example.com" {
		t.Fatalf("BaseURL = %q", cfg.BaseURL)
	}
	if cfg.APIKey != "sk-test" {
		t.Fatalf("APIKey = %q", cfg.APIKey)
	}
	if cfg.Model != "gpt-test" {
		t.Fatalf("Model = %q", cfg.Model)
	}
	if cfg.Temperature != 0.3 {
		t.Fatalf("Temperature = %v", cfg.Temperature)
	}
	if cfg.MaxTokens != 1024 {
		t.Fatalf("MaxTokens = %v", cfg.MaxTokens)
	}
	if !cfg.Enabled() {
		t.Fatal("Enabled should be true when all required env set")
	}
}

func TestFromEnvDefaults(t *testing.T) {
	t.Setenv("MILES_PROVIDER_BASE_URL", "")
	t.Setenv("MILES_PROVIDER_API_KEY", "")
	t.Setenv("MILES_PROVIDER_MODEL", "")
	t.Setenv("MILES_PROVIDER_TEMPERATURE", "")
	t.Setenv("MILES_PROVIDER_MAX_TOKENS", "")

	cfg := config.FromEnv()

	if cfg.Temperature != 0.7 {
		t.Fatalf("default Temperature should be 0.7, got %v", cfg.Temperature)
	}
	if cfg.MaxTokens != 2048 {
		t.Fatalf("default MaxTokens should be 2048, got %v", cfg.MaxTokens)
	}
	if cfg.Enabled() {
		t.Fatal("Enabled should be false when required env missing")
	}
}

func TestEnabledRequiresAllThree(t *testing.T) {
	cases := []struct {
		name              string
		baseURL, key, mdl string
		want              bool
	}{
		{"all set", "https://x", "k", "m", true},
		{"missing baseURL", "", "k", "m", false},
		{"missing key", "https://x", "", "m", false},
		{"missing model", "https://x", "k", "", false},
		{"whitespace only", "  ", "k", "m", false},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			t.Setenv("MILES_PROVIDER_BASE_URL", tc.baseURL)
			t.Setenv("MILES_PROVIDER_API_KEY", tc.key)
			t.Setenv("MILES_PROVIDER_MODEL", tc.mdl)
			if got := config.FromEnv().Enabled(); got != tc.want {
				t.Fatalf("Enabled() = %v, want %v", got, tc.want)
			}
		})
	}
}
```

- [ ] **Step 2: Run to verify it fails**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/config/...)
```

Expected: build fails with `no Go files in .../config` or similar.

- [ ] **Step 3: Implement config.go**

Create `apps/agent-core/internal/chat/config/config.go`:

```go
// Package config exposes provider configuration sourced from environment variables.
package config

import (
	"os"
	"strconv"
	"strings"
)

type ProviderConfig struct {
	BaseURL     string
	APIKey      string
	Model       string
	Temperature float64
	MaxTokens   int
}

const (
	defaultTemperature = 0.7
	defaultMaxTokens   = 2048
)

func FromEnv() ProviderConfig {
	return ProviderConfig{
		BaseURL:     strings.TrimSpace(os.Getenv("MILES_PROVIDER_BASE_URL")),
		APIKey:      strings.TrimSpace(os.Getenv("MILES_PROVIDER_API_KEY")),
		Model:       strings.TrimSpace(os.Getenv("MILES_PROVIDER_MODEL")),
		Temperature: parseFloat(os.Getenv("MILES_PROVIDER_TEMPERATURE"), defaultTemperature),
		MaxTokens:   parseInt(os.Getenv("MILES_PROVIDER_MAX_TOKENS"), defaultMaxTokens),
	}
}

func (c ProviderConfig) Enabled() bool {
	return c.BaseURL != "" && c.APIKey != "" && c.Model != ""
}

func parseFloat(s string, def float64) float64 {
	if strings.TrimSpace(s) == "" {
		return def
	}
	v, err := strconv.ParseFloat(strings.TrimSpace(s), 64)
	if err != nil {
		return def
	}
	return v
}

func parseInt(s string, def int) int {
	if strings.TrimSpace(s) == "" {
		return def
	}
	v, err := strconv.Atoi(strings.TrimSpace(s))
	if err != nil {
		return def
	}
	return v
}
```

- [ ] **Step 4: Run to verify it passes**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/config/...)
```

Expected: `ok milesedgeworth/agent-core/internal/chat/config`.

- [ ] **Step 5: Commit**

```bash
git add apps/agent-core/internal/chat/config/
git commit -m "feat(agent-core): provider config from env vars"
```

## Task 3: Expression Parser — Happy Path

**Files:**
- Create: `apps/agent-core/internal/chat/expression/parser.go`
- Create: `apps/agent-core/internal/chat/expression/parser_test.go`

- [ ] **Step 1: Write the failing test for the basic cases**

Create `apps/agent-core/internal/chat/expression/parser_test.go`:

```go
package expression_test

import (
	"strings"
	"testing"

	"milesedgeworth/agent-core/internal/chat/expression"
)

type capture struct {
	out strings.Builder
}

func (c *capture) text(s string)      { c.out.WriteString("T(" + s + ")") }
func (c *capture) expr(tag string)    { c.out.WriteString("E(" + tag + ")") }
func (c *capture) string() string     { return c.out.String() }

func newParser(c *capture, known []string) *expression.Parser {
	return &expression.Parser{
		KnownTags:   known,
		FallbackTag: "neutral",
		OnText:      c.text,
		OnExpression: c.expr,
	}
}

func TestParserPlainText(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection", "polite", "neutral"})
	p.Feed("hello world")
	p.Flush()
	if got := cap.string(); got != "T(hello world)" {
		t.Fatalf("got %q", got)
	}
}

func TestParserMarkerAtStart(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection", "polite", "neutral"})
	p.Feed("[EXPR:objection]异议！")
	p.Flush()
	if got := cap.string(); got != "E(objection)T(异议！)" {
		t.Fatalf("got %q", got)
	}
}

func TestParserMarkerInMiddle(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection", "polite", "neutral"})
	p.Feed("hello [EXPR:polite]world")
	p.Flush()
	if got := cap.string(); got != "T(hello )E(polite)T(world)" {
		t.Fatalf("got %q", got)
	}
}

func TestParserMultipleMarkers(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection", "polite", "neutral"})
	p.Feed("[EXPR:objection]一段[EXPR:polite]二段")
	p.Flush()
	if got := cap.string(); got != "E(objection)T(一段)E(polite)T(二段)" {
		t.Fatalf("got %q", got)
	}
}
```

- [ ] **Step 2: Run to verify it fails**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/expression/...)
```

Expected: build fails with no Go files.

- [ ] **Step 3: Implement parser.go**

Create `apps/agent-core/internal/chat/expression/parser.go`:

```go
// Package expression splits a streaming token sequence into TEXT and EXPR events.
// Markers have the form [EXPR:tag_name]. Unknown tags are downgraded to FallbackTag.
package expression

import "strings"

const (
	markerOpen  = "[EXPR:"
	markerClose = "]"
)

// Parser is single-pass and not safe for concurrent use.
// The caller fills KnownTags, FallbackTag, OnText, OnExpression before Feed.
type Parser struct {
	KnownTags    []string
	FallbackTag  string
	OnText       func(string)
	OnExpression func(string)

	known  map[string]bool
	buffer string
}

func (p *Parser) ensureKnown() {
	if p.known != nil {
		return
	}
	p.known = make(map[string]bool, len(p.KnownTags))
	for _, t := range p.KnownTags {
		p.known[t] = true
	}
}

// Feed appends chunk and emits any complete text/marker events.
// Unparseable suffix is held in buffer until the next Feed or Flush.
func (p *Parser) Feed(chunk string) {
	p.ensureKnown()
	p.buffer += chunk
	for p.consume() {
	}
}

// Flush emits any held buffer as text. Call once after the stream ends.
func (p *Parser) Flush() {
	if p.buffer == "" {
		return
	}
	if p.OnText != nil {
		p.OnText(p.buffer)
	}
	p.buffer = ""
}

// consume tries to emit one event. Returns true if it made progress.
func (p *Parser) consume() bool {
	idx := strings.Index(p.buffer, markerOpen)
	if idx == -1 {
		// No marker possible; emit safe prefix, hold ambiguous tail.
		safe, tail := splitSafeForPrefix(p.buffer, markerOpen)
		if safe != "" && p.OnText != nil {
			p.OnText(safe)
		}
		p.buffer = tail
		return false
	}

	if idx > 0 {
		if p.OnText != nil {
			p.OnText(p.buffer[:idx])
		}
		p.buffer = p.buffer[idx:]
	}

	// buffer now starts with markerOpen.
	endIdx := strings.Index(p.buffer, markerClose)
	if endIdx == -1 {
		// Marker incomplete; wait for more chunks.
		return false
	}

	tag := p.buffer[len(markerOpen):endIdx]
	p.buffer = p.buffer[endIdx+len(markerClose):]

	if p.OnExpression != nil {
		if p.known[tag] {
			p.OnExpression(tag)
		} else {
			p.OnExpression(p.FallbackTag)
		}
	}
	return true
}

// splitSafeForPrefix returns (safe, tail) where tail is any suffix of s that
// could still grow into prefix. Used to decide what to flush vs hold when no
// complete marker is found yet.
func splitSafeForPrefix(s, prefix string) (string, string) {
	maxK := len(prefix) - 1
	if maxK > len(s) {
		maxK = len(s)
	}
	for k := maxK; k > 0; k-- {
		if strings.HasPrefix(prefix, s[len(s)-k:]) {
			return s[:len(s)-k], s[len(s)-k:]
		}
	}
	return s, ""
}
```

- [ ] **Step 4: Run to verify it passes**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/expression/...)
```

Expected: 4 tests pass.

- [ ] **Step 5: Commit**

```bash
git add apps/agent-core/internal/chat/expression/
git commit -m "feat(agent-core): EXPR marker parser happy-path"
```

## Task 4: Expression Parser — Edge Cases

**Files:**
- Modify: `apps/agent-core/internal/chat/expression/parser_test.go`

- [ ] **Step 1: Append edge-case tests**

Append to `apps/agent-core/internal/chat/expression/parser_test.go`:

```go
func TestParserCrossChunk(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection", "polite", "neutral"})
	p.Feed("abc[EX")
	p.Feed("PR:polite]def")
	p.Flush()
	if got := cap.string(); got != "T(abc)E(polite)T(def)" {
		t.Fatalf("got %q", got)
	}
}

func TestParserCrossChunkVeryFragmented(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection"})
	// One rune at a time (worst case for streaming providers).
	for _, r := range "[EXPR:objection]hi" {
		p.Feed(string(r))
	}
	p.Flush()
	if got := cap.string(); got != "E(objection)T(h)T(i)" {
		t.Fatalf("got %q", got)
	}
}

func TestParserUnknownTag(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection", "neutral"})
	p.Feed("[EXPR:mystery]hi")
	p.Flush()
	if got := cap.string(); got != "E(neutral)T(hi)" {
		t.Fatalf("got %q", got)
	}
}

func TestParserHalfMarkerAtEnd(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection"})
	p.Feed("text[EX")
	p.Flush()
	// On flush the held "[EX" surfaces as text rather than being lost.
	if got := cap.string(); got != "T(text)T([EX)" {
		t.Fatalf("got %q", got)
	}
}

func TestParserNonMarkerBracket(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection"})
	p.Feed("array[0]=1")
	p.Flush()
	if got := cap.string(); got != "T(array[0]=1)" {
		t.Fatalf("got %q", got)
	}
}

func TestParserEmptyTagFallsBack(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, []string{"objection", "neutral"})
	p.Feed("[EXPR:]hi")
	p.Flush()
	if got := cap.string(); got != "E(neutral)T(hi)" {
		t.Fatalf("got %q", got)
	}
}
```

- [ ] **Step 2: Run to verify**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/expression/...)
```

Expected: 10 tests pass. If `TestParserCrossChunkVeryFragmented` fails because `Feed("h")` and `Feed("i")` emit two separate `T()` events, that's the intended behavior (the parser doesn't coalesce text fragments; the caller decides what to do with them).

- [ ] **Step 3: Commit**

```bash
git add apps/agent-core/internal/chat/expression/parser_test.go
git commit -m "test(agent-core): EXPR parser cross-chunk and edge cases"
```

## Task 5: Request Struct JSON Tags + ExpressionInfo

**Files:**
- Modify: `apps/agent-core/internal/chat/provider.go`
- Modify: `apps/agent-core/internal/api/server_test.go`

- [ ] **Step 1: Write a new failing test for expression field**

Append to `apps/agent-core/internal/api/server_test.go`:

```go
func TestChatStreamForwardsExpressions(t *testing.T) {
	server := httptest.NewServer(api.NewServer(chat.NewMockProvider(0), "mock").Routes())
	defer server.Close()

	payload := []byte(`{
		"conversationId": "default",
		"message": "hi",
		"expressions": [
			{"id": "objection", "description": "strong rebuttal"},
			{"id": "polite"}
		]
	}`)
	resp, err := http.Post(server.URL+"/v1/chat/messages", "application/json", bytes.NewReader(payload))
	if err != nil {
		t.Fatalf("POST failed: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}
	// Mock doesn't currently echo expressions; we just verify decoding doesn't blow up.
}
```

Note: this test also expects `api.NewServer` to accept a second `string` label argument (added in Task 9). For now compile will fail on that — which is fine, it will guide Task 9.

- [ ] **Step 2: Run to verify the test fails**

Run:

```bash
(cd apps/agent-core && go test ./internal/api/...)
```

Expected: compile error about `api.NewServer` argument count, or about `expressions` field if you reorder. That's expected.

- [ ] **Step 3: Update provider.go with JSON tags and ExpressionInfo**

Replace `apps/agent-core/internal/chat/provider.go` with:

```go
package chat

import "context"

// ExpressionInfo describes one expression tag the current skin makes available
// to the model. Sent from Qt to the sidecar with each chat request.
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
	StreamReply(ctx context.Context, req Request) (<-chan StreamEvent, error)
}
```

- [ ] **Step 4: Confirm existing tests still compile against unchanged server signature**

Run:

```bash
(cd apps/agent-core && go build ./...)
```

Expected: compile fails on `TestChatStreamForwardsExpressions` because `api.NewServer` still takes one arg. That's expected — Task 9 fixes it. For now, **revert just the new test** to a placeholder that doesn't reference the second arg:

Comment out the test body temporarily (leave the test function in place):

```go
func TestChatStreamForwardsExpressions(t *testing.T) {
	t.Skip("enabled in Task 9 once api.NewServer accepts a provider label")
}
```

This keeps the file compilable through intermediate tasks.

- [ ] **Step 5: Run all current Go tests**

Run:

```bash
(cd apps/agent-core && go test ./...)
```

Expected: all green (mock provider tests still pass with new JSON tags because Go's struct-tag matching is backward compatible).

- [ ] **Step 6: Commit**

```bash
git add apps/agent-core/internal/chat/provider.go apps/agent-core/internal/api/server_test.go
git commit -m "feat(agent-core): json tags + ExpressionInfo on Request"
```

## Task 6: OpenAI Provider — System Prompt Builder

**Files:**
- Create: `apps/agent-core/internal/chat/openai/provider.go` (initial skeleton)
- Create: `apps/agent-core/internal/chat/openai/provider_test.go` (system prompt only)

- [ ] **Step 1: Write failing test for buildSystemPrompt**

Create `apps/agent-core/internal/chat/openai/provider_test.go`:

```go
package openai_test

import (
	"strings"
	"testing"

	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/openai"
)

func TestBuildSystemPromptIncludesAllExpressions(t *testing.T) {
	exprs := []chat.ExpressionInfo{
		{ID: "neutral", Description: "默认状态"},
		{ID: "objection", Description: "强烈反驳"},
		{ID: "polite"},
	}
	prompt := openai.BuildSystemPrompt(exprs)

	for _, expect := range []string{
		"[EXPR:",
		"neutral",
		"默认状态",
		"objection",
		"强烈反驳",
		"polite",
	} {
		if !strings.Contains(prompt, expect) {
			t.Fatalf("prompt missing %q\n---\n%s", expect, prompt)
		}
	}
}

func TestBuildSystemPromptHandlesEmptyExpressions(t *testing.T) {
	prompt := openai.BuildSystemPrompt(nil)
	// Still emits an instruction line, just without a tag list.
	if !strings.Contains(prompt, "[EXPR:") {
		t.Fatalf("prompt should explain markers even with no tags\n---\n%s", prompt)
	}
}
```

- [ ] **Step 2: Run to verify it fails**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/openai/...)
```

Expected: package not found.

- [ ] **Step 3: Implement minimal provider.go with BuildSystemPrompt**

Create `apps/agent-core/internal/chat/openai/provider.go`:

```go
// Package openai implements an OpenAI-compatible Chat Completions provider.
package openai

import (
	"strings"

	"milesedgeworth/agent-core/internal/chat"
)

// BuildSystemPrompt assembles the persona + expression instruction shown to the model.
// Phase 2.3 will replace this with the full Miles persona; Phase 2.1 only ships the
// minimal instruction needed for the [EXPR:tag] protocol to function.
func BuildSystemPrompt(expressions []chat.ExpressionInfo) string {
	var sb strings.Builder
	sb.WriteString("你是 Miles Edgeworth 桌宠助手。回复时在每段文字开头用 [EXPR:标签名] 标记当前表达。")
	sb.WriteString("情绪延续时不重复标记。回复的第一段文字必须有标记。\n\n")
	if len(expressions) == 0 {
		return sb.String()
	}
	sb.WriteString("当前可用表达标签：\n")
	for _, e := range expressions {
		sb.WriteString("- ")
		sb.WriteString(e.ID)
		if e.Description != "" {
			sb.WriteString("：")
			sb.WriteString(e.Description)
		}
		sb.WriteString("\n")
	}
	return sb.String()
}
```

- [ ] **Step 4: Run to verify it passes**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/openai/...)
```

Expected: 2 tests pass.

- [ ] **Step 5: Commit**

```bash
git add apps/agent-core/internal/chat/openai/
git commit -m "feat(agent-core): openai BuildSystemPrompt"
```

## Task 7: OpenAI Provider — StreamReply Happy Path

**Files:**
- Modify: `apps/agent-core/internal/chat/openai/provider.go`
- Modify: `apps/agent-core/internal/chat/openai/provider_test.go`

- [ ] **Step 1: Write failing httptest-based test**

Append to `apps/agent-core/internal/chat/openai/provider_test.go`:

```go
import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
)

func TestStreamReplyHappyPath(t *testing.T) {
	// Upstream OpenAI-style SSE: deltas containing [EXPR:objection]...
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/v1/chat/completions" {
			t.Fatalf("upstream got %s", r.URL.Path)
		}
		var body struct {
			Model    string `json:"model"`
			Messages []struct {
				Role    string `json:"role"`
				Content string `json:"content"`
			} `json:"messages"`
			Stream bool `json:"stream"`
		}
		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			t.Fatalf("upstream decode: %v", err)
		}
		if !body.Stream || body.Model != "test-model" {
			t.Fatalf("unexpected upstream body: %+v", body)
		}
		if len(body.Messages) != 2 || body.Messages[0].Role != "system" {
			t.Fatalf("expected system+user messages, got %+v", body.Messages)
		}

		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		flusher := w.(http.Flusher)

		chunks := []string{
			`{"choices":[{"delta":{"content":"[EXPR:objection]"}}]}`,
			`{"choices":[{"delta":{"content":"异议！"}}]}`,
			`{"choices":[{"delta":{"content":"[EXPR:polite]"}}]}`,
			`{"choices":[{"delta":{"content":"再见。"}}]}`,
			`[DONE]`,
		}
		for _, c := range chunks {
			fmt.Fprintf(w, "data: %s\n\n", c)
			flusher.Flush()
		}
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-test", "test-model", 0.5, 256)
	events, err := p.StreamReply(context.Background(), chat.Request{
		ConversationID: "c1",
		Message:        "你怎么看？",
		Expressions: []chat.ExpressionInfo{
			{ID: "neutral"},
			{ID: "objection"},
			{ID: "polite"},
		},
	})
	if err != nil {
		t.Fatalf("StreamReply: %v", err)
	}

	var got []chat.StreamEvent
	for e := range events {
		got = append(got, e)
	}

	mustFind := func(predicate func(chat.StreamEvent) bool, what string) {
		t.Helper()
		for _, e := range got {
			if predicate(e) {
				return
			}
		}
		t.Fatalf("missing event: %s\nall: %+v", what, got)
	}

	mustFind(func(e chat.StreamEvent) bool { return e.Type == "RUN_STARTED" }, "RUN_STARTED")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "CUSTOM" && e.Name == "miles.pet.expression.requested" &&
			e.Value["state"] == "thinking"
	}, "thinking expression")
	mustFind(func(e chat.StreamEvent) bool { return e.Type == "TEXT_MESSAGE_START" }, "TEXT_MESSAGE_START")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "CUSTOM" && e.Value["state"] == "speaking" && e.Value["expression"] == "objection"
	}, "speaking objection expression")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "TEXT_MESSAGE_CONTENT" && e.Delta == "异议！"
	}, "objection text")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "CUSTOM" && e.Value["state"] == "speaking" && e.Value["expression"] == "polite"
	}, "speaking polite expression")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "TEXT_MESSAGE_CONTENT" && e.Delta == "再见。"
	}, "polite text")
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "CUSTOM" && e.Value["state"] == "idle" &&
			e.Value["interruptHint"] == "afterCurrent"
	}, "idle afterCurrent expression")
	mustFind(func(e chat.StreamEvent) bool { return e.Type == "RUN_FINISHED" }, "RUN_FINISHED")
}
```

- [ ] **Step 2: Run to verify it fails**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/openai/...)
```

Expected: compile fails on `openai.NewProvider` (not yet defined).

- [ ] **Step 3: Implement NewProvider and StreamReply**

Replace `apps/agent-core/internal/chat/openai/provider.go` with:

```go
// Package openai implements an OpenAI-compatible Chat Completions provider.
package openai

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/expression"
)

type Provider struct {
	baseURL     string
	apiKey      string
	model       string
	temperature float64
	maxTokens   int
	httpClient  *http.Client
}

func NewProvider(baseURL, apiKey, model string, temperature float64, maxTokens int) *Provider {
	return &Provider{
		baseURL:     strings.TrimRight(baseURL, "/"),
		apiKey:      apiKey,
		model:       model,
		temperature: temperature,
		maxTokens:   maxTokens,
		httpClient:  &http.Client{Timeout: 120 * time.Second},
	}
}

func BuildSystemPrompt(expressions []chat.ExpressionInfo) string {
	var sb strings.Builder
	sb.WriteString("你是 Miles Edgeworth 桌宠助手。回复时在每段文字开头用 [EXPR:标签名] 标记当前表达。")
	sb.WriteString("情绪延续时不重复标记。回复的第一段文字必须有标记。\n\n")
	if len(expressions) == 0 {
		return sb.String()
	}
	sb.WriteString("当前可用表达标签：\n")
	for _, e := range expressions {
		sb.WriteString("- ")
		sb.WriteString(e.ID)
		if e.Description != "" {
			sb.WriteString("：")
			sb.WriteString(e.Description)
		}
		sb.WriteString("\n")
	}
	return sb.String()
}

type chatMessage struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

type chatCompletionRequest struct {
	Model       string        `json:"model"`
	Messages    []chatMessage `json:"messages"`
	Stream      bool          `json:"stream"`
	Temperature float64       `json:"temperature,omitempty"`
	MaxTokens   int           `json:"max_tokens,omitempty"`
}

type chatCompletionStreamChunk struct {
	Choices []struct {
		Delta struct {
			Content string `json:"content"`
		} `json:"delta"`
	} `json:"choices"`
}

const (
	runID     = "openai-run-1"
	messageID = "openai-msg-1"
)

func (p *Provider) StreamReply(ctx context.Context, req chat.Request) (<-chan chat.StreamEvent, error) {
	events := make(chan chat.StreamEvent, 32)

	body := chatCompletionRequest{
		Model: p.model,
		Messages: []chatMessage{
			{Role: "system", Content: BuildSystemPrompt(req.Expressions)},
			{Role: "user", Content: req.Message},
		},
		Stream:      true,
		Temperature: p.temperature,
		MaxTokens:   p.maxTokens,
	}
	encoded, err := json.Marshal(body)
	if err != nil {
		close(events)
		return events, err
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost,
		p.baseURL+"/v1/chat/completions", bytes.NewReader(encoded))
	if err != nil {
		close(events)
		return events, err
	}
	httpReq.Header.Set("Content-Type", "application/json")
	httpReq.Header.Set("Authorization", "Bearer "+p.apiKey)
	httpReq.Header.Set("Accept", "text/event-stream")

	resp, err := p.httpClient.Do(httpReq)
	if err != nil {
		close(events)
		return events, err
	}

	if resp.StatusCode != http.StatusOK {
		// Drain a small sample for diagnostics, then close.
		sample, _ := io.ReadAll(io.LimitReader(resp.Body, 512))
		resp.Body.Close()
		go func() {
			defer close(events)
			send(ctx, events, chat.StreamEvent{
				Type:  "RUN_ERROR",
				Error: fmt.Sprintf("provider returned %d: %s", resp.StatusCode, strings.TrimSpace(string(sample))),
			})
		}()
		return events, nil
	}

	knownTags := make([]string, 0, len(req.Expressions))
	for _, e := range req.Expressions {
		knownTags = append(knownTags, e.ID)
	}

	go p.pipe(ctx, resp, events, knownTags)
	return events, nil
}

func (p *Provider) pipe(ctx context.Context, resp *http.Response, events chan<- chat.StreamEvent, knownTags []string) {
	defer resp.Body.Close()
	defer close(events)

	if !send(ctx, events, chat.StreamEvent{Type: "RUN_STARTED", RunID: runID}) {
		return
	}
	if !send(ctx, events, chat.StreamEvent{
		Type:  "CUSTOM",
		Name:  "miles.pet.expression.requested",
		RunID: runID,
		Value: map[string]any{"state": "thinking", "expression": "neutral"},
	}) {
		return
	}
	if !send(ctx, events, chat.StreamEvent{
		Type:      "TEXT_MESSAGE_START",
		RunID:     runID,
		MessageID: messageID,
		Role:      "assistant",
	}) {
		return
	}

	sawFirstSpeaking := false
	parser := &expression.Parser{
		KnownTags:   knownTags,
		FallbackTag: "neutral",
		OnExpression: func(tag string) {
			sawFirstSpeaking = true
			send(ctx, events, chat.StreamEvent{
				Type:  "CUSTOM",
				Name:  "miles.pet.expression.requested",
				RunID: runID,
				Value: map[string]any{"state": "speaking", "expression": tag},
			})
		},
		OnText: func(text string) {
			if !sawFirstSpeaking {
				sawFirstSpeaking = true
				send(ctx, events, chat.StreamEvent{
					Type:  "CUSTOM",
					Name:  "miles.pet.expression.requested",
					RunID: runID,
					Value: map[string]any{"state": "speaking", "expression": "neutral"},
				})
			}
			send(ctx, events, chat.StreamEvent{
				Type:      "TEXT_MESSAGE_CONTENT",
				RunID:     runID,
				MessageID: messageID,
				Delta:     text,
			})
		},
	}

	scanner := bufio.NewScanner(resp.Body)
	scanner.Buffer(make([]byte, 0, 64*1024), 1024*1024)
	for scanner.Scan() {
		line := scanner.Text()
		if !strings.HasPrefix(line, "data:") {
			continue
		}
		payload := strings.TrimSpace(strings.TrimPrefix(line, "data:"))
		if payload == "[DONE]" {
			break
		}
		var chunk chatCompletionStreamChunk
		if err := json.Unmarshal([]byte(payload), &chunk); err != nil {
			continue
		}
		for _, choice := range chunk.Choices {
			if choice.Delta.Content != "" {
				parser.Feed(choice.Delta.Content)
			}
		}
	}
	parser.Flush()

	send(ctx, events, chat.StreamEvent{
		Type:      "TEXT_MESSAGE_END",
		RunID:     runID,
		MessageID: messageID,
	})
	send(ctx, events, chat.StreamEvent{
		Type:  "CUSTOM",
		Name:  "miles.pet.expression.requested",
		RunID: runID,
		Value: map[string]any{
			"state":         "idle",
			"expression":    "neutral",
			"interruptHint": "afterCurrent",
		},
	})
	send(ctx, events, chat.StreamEvent{Type: "RUN_FINISHED", RunID: runID})
}

func send(ctx context.Context, events chan<- chat.StreamEvent, e chat.StreamEvent) bool {
	select {
	case <-ctx.Done():
		return false
	case events <- e:
		return true
	}
}
```

- [ ] **Step 4: Run to verify happy-path test passes**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/openai/...)
```

Expected: 3 tests pass.

- [ ] **Step 5: Commit**

```bash
git add apps/agent-core/internal/chat/openai/
git commit -m "feat(agent-core): openai provider StreamReply with EXPR parser"
```

## Task 8: OpenAI Provider — Error Paths

**Files:**
- Modify: `apps/agent-core/internal/chat/openai/provider_test.go`

- [ ] **Step 1: Append error-path test**

Append to `apps/agent-core/internal/chat/openai/provider_test.go`:

```go
func TestStreamReply4xxBecomesRunError(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, `{"error":{"message":"invalid api key"}}`, http.StatusUnauthorized)
	}))
	defer upstream.Close()

	p := openai.NewProvider(upstream.URL, "sk-bad", "any", 0.7, 128)
	events, err := p.StreamReply(context.Background(), chat.Request{Message: "hi"})
	if err != nil {
		t.Fatalf("StreamReply returned err: %v", err)
	}
	var got []chat.StreamEvent
	for e := range events {
		got = append(got, e)
	}
	if len(got) != 1 || got[0].Type != "RUN_ERROR" {
		t.Fatalf("expected single RUN_ERROR, got %+v", got)
	}
	if !strings.Contains(got[0].Error, "401") {
		t.Fatalf("RUN_ERROR should mention status code, got %q", got[0].Error)
	}
}

func TestStreamReplySkipsMalformedChunks(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		flusher := w.(http.Flusher)
		for _, line := range []string{
			"data: {not-json}",
			`data: {"choices":[{"delta":{"content":"[EXPR:polite]ok"}}]}`,
			"data: [DONE]",
		} {
			fmt.Fprintf(w, "%s\n\n", line)
			flusher.Flush()
		}
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
	gotPolite := false
	for e := range events {
		if e.Type == "CUSTOM" && e.Value["expression"] == "polite" {
			gotPolite = true
		}
	}
	if !gotPolite {
		t.Fatal("expected polite expression after malformed chunk was skipped")
	}
}
```

- [ ] **Step 2: Run to verify both tests pass**

Run:

```bash
(cd apps/agent-core && go test ./internal/chat/openai/...)
```

Expected: 5 tests pass total.

- [ ] **Step 3: Commit**

```bash
git add apps/agent-core/internal/chat/openai/provider_test.go
git commit -m "test(agent-core): openai provider error and malformed chunk paths"
```

## Task 9: Server Hardening + Provider Selection in main.go

**Files:**
- Modify: `apps/agent-core/internal/api/server.go`
- Modify: `apps/agent-core/internal/api/server_test.go`
- Modify: `apps/agent-core/cmd/miles-agent/main.go`

- [ ] **Step 1: Re-enable skipped test from Task 5**

In `apps/agent-core/internal/api/server_test.go`, replace the placeholder `TestChatStreamForwardsExpressions` body (currently `t.Skip(...)`) with the real body written in Task 5 step 1.

Also update existing test calls to `api.NewServer(...)` to pass a label as second argument:

Find every:

```go
api.NewServer(chat.NewMockProvider(0))
```

Replace with:

```go
api.NewServer(chat.NewMockProvider(0), "mock")
```

(And similarly for non-zero delays.) There are three call sites in `server_test.go`.

- [ ] **Step 2: Add a test for MaxBytesReader**

Append:

```go
func TestChatStreamRejectsOversizedBody(t *testing.T) {
	server := httptest.NewServer(api.NewServer(chat.NewMockProvider(0), "mock").Routes())
	defer server.Close()

	huge := strings.Repeat("x", 2*1024*1024)
	body := fmt.Sprintf(`{"message":"%s"}`, huge)
	resp, err := http.Post(server.URL+"/v1/chat/messages", "application/json", strings.NewReader(body))
	if err != nil {
		t.Fatalf("POST: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusRequestEntityTooLarge && resp.StatusCode != http.StatusBadRequest {
		t.Fatalf("status = %d, want 413 or 400", resp.StatusCode)
	}
}
```

Make sure `"fmt"` is imported in this test file (it likely already is).

- [ ] **Step 3: Run to verify failures**

Run:

```bash
(cd apps/agent-core && go test ./internal/api/...)
```

Expected: compile errors about argument count + the new tests fail since MaxBytesReader isn't wired up.

- [ ] **Step 4: Update server.go**

Replace `apps/agent-core/internal/api/server.go` with:

```go
package api

import (
	"encoding/json"
	"net/http"
	"strings"

	"milesedgeworth/agent-core/internal/chat"
)

const (
	// DefaultListenAddr is the localhost:port the desktop app expects the sidecar on.
	// Kept in sync with apps/desktop/src/chat/ChatController.cpp.
	DefaultListenAddr = "127.0.0.1:39710"

	// MaxRequestBodyBytes caps incoming chat request bodies to avoid runaway upstreams
	// or accidental large payloads pinning sidecar memory.
	MaxRequestBodyBytes = 1 * 1024 * 1024
)

type Server struct {
	provider      chat.Provider
	providerLabel string
}

func NewServer(provider chat.Provider, providerLabel string) *Server {
	if providerLabel == "" {
		providerLabel = "unknown"
	}
	return &Server{provider: provider, providerLabel: providerLabel}
}

func (s *Server) Routes() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/health", s.handleHealth)
	mux.HandleFunc("/v1/chat/messages", s.handleChatMessages)
	return mux
}

func (s *Server) handleHealth(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{
		"ok":       true,
		"provider": s.providerLabel,
		"service":  "miles-agent",
	})
}

func (s *Server) handleChatMessages(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	r.Body = http.MaxBytesReader(w, r.Body, MaxRequestBodyBytes)
	var req chat.Request
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]any{"error": "invalid JSON"})
		return
	}
	req.Message = strings.TrimSpace(req.Message)
	if req.Message == "" {
		writeJSON(w, http.StatusBadRequest, map[string]any{"error": "message is required"})
		return
	}

	events, err := s.provider.StreamReply(r.Context(), req)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]any{"error": err.Error()})
		return
	}

	w.Header().Set("Content-Type", "text/event-stream; charset=utf-8")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.WriteHeader(http.StatusOK)

	flusher, _ := w.(http.Flusher)
	for event := range events {
		if err := writeSSE(w, event); err != nil {
			return
		}
		if flusher != nil {
			flusher.Flush()
		}
	}
}

func writeJSON(w http.ResponseWriter, status int, body any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(body)
}

func writeSSE(w http.ResponseWriter, event chat.StreamEvent) error {
	body, err := json.Marshal(event)
	if err != nil {
		return err
	}
	_, err = w.Write(append(append([]byte("data: "), body...), '\n', '\n'))
	return err
}
```

- [ ] **Step 5: Update main.go to pick provider**

Replace `apps/agent-core/cmd/miles-agent/main.go` with:

```go
package main

import (
	"context"
	"flag"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"milesedgeworth/agent-core/internal/api"
	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/config"
	"milesedgeworth/agent-core/internal/chat/openai"
)

func main() {
	addr := flag.String("addr", api.DefaultListenAddr, "listen address")
	flag.Parse()

	cfg := config.FromEnv()
	var provider chat.Provider
	label := "mock-fallback"
	if cfg.Enabled() {
		provider = openai.NewProvider(cfg.BaseURL, cfg.APIKey, cfg.Model, cfg.Temperature, cfg.MaxTokens)
		label = "openai-compatible"
		log.Printf("provider: openai-compatible model=%s", cfg.Model)
	} else {
		provider = chat.NewMockProvider(35 * time.Millisecond)
		log.Printf("provider: mock-fallback (set MILES_PROVIDER_BASE_URL, MILES_PROVIDER_API_KEY, MILES_PROVIDER_MODEL to use a real provider)")
	}

	server := &http.Server{
		Addr:    *addr,
		Handler: api.NewServer(provider, label).Routes(),
	}

	errs := make(chan error, 1)
	go func() {
		log.Printf("miles-agent listening on %s", *addr)
		errs <- server.ListenAndServe()
	}()

	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt, syscall.SIGTERM)

	select {
	case sig := <-signals:
		log.Printf("received %s, shutting down", sig)
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		if err := server.Shutdown(ctx); err != nil {
			log.Fatalf("shutdown failed: %v", err)
		}
	case err := <-errs:
		if err != nil && err != http.ErrServerClosed {
			log.Fatalf("server failed: %v", err)
		}
	}
}
```

- [ ] **Step 6: Run all Go tests**

Run:

```bash
(cd apps/agent-core && go test ./...)
```

Expected: all green.

- [ ] **Step 7: Commit**

```bash
git add apps/agent-core/
git commit -m "feat(agent-core): provider selection, MaxBytesReader, shared listen addr"
```

## Task 10: Qt ChatController — Hold Buffer Plumbing

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Add smoke test that exercises hold buffer**

In `apps/desktop/tests/chat_controller_smoke.cpp`, **just before** the final `return 0;`, append:

```cpp
    // Hold buffer round-trip: feed a delta while the controller is in its default
    // "immediate flush" mode (Phase 2.1 plumbing — full GATED logic lands in 2.3).
    PetRuntime hbRuntime;
    ChatController hbController(&hbRuntime);

    ChatStreamEvent hbStarted;
    hbStarted.type = QStringLiteral("RUN_STARTED");
    hbController.applyStreamEvent(hbStarted);

    ChatStreamEvent hbStart;
    hbStart.type = QStringLiteral("TEXT_MESSAGE_START");
    hbStart.role = QStringLiteral("assistant");
    hbController.applyStreamEvent(hbStart);

    ChatStreamEvent hbContent;
    hbContent.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
    hbContent.delta = QStringLiteral("片段一");
    hbController.applyStreamEvent(hbContent);

    require(hbController.messages().constFirst().toMap().value("text").toString()
                == QStringLiteral("片段一"),
            "hold buffer should flush content immediately in phase 2.1");

    hbContent.delta = QStringLiteral("片段二");
    hbController.applyStreamEvent(hbContent);
    require(hbController.messages().constFirst().toMap().value("text").toString()
                == QStringLiteral("片段一片段二"),
            "subsequent deltas should append through the hold buffer");
```

- [ ] **Step 2: Run to verify it fails to compile**

Run:

```bash
cmake --build build --target chat_controller_smoke 2>&1 | tail -20
```

Expected: smoke binary builds fine (just runs existing flow). It will pass since the existing append flow does what the new asserts check. **However** the contract test will fail because `m_holdBuffer` is not yet in the header. Verify:

```bash
python3 tests/check_phase_2_1_provider.py 2>&1 | tail -5
```

Expected: assertion about `m_holdBuffer` missing.

- [ ] **Step 3: Update ChatController.h**

In `apps/desktop/src/chat/ChatController.h`, find the existing private member block (around line 69-82). Add two lines.

Find:

```cpp
    QVariantList m_messages;
    bool m_sidecarReady = false;
```

Replace with:

```cpp
    QVariantList m_messages;
    // Phase 2.1 plumbing: accumulate streamed deltas before pushing to m_messages.
    // Phase 2.3 will gate this buffer on PetRuntime animation boundaries; for now
    // every Feed flushes immediately, matching the previous direct-append behavior.
    QString m_holdBuffer;
    bool m_sidecarReady = false;
```

In the private methods declaration block (around line 65-67), find:

```cpp
    void appendAssistantDelta(const QString &delta);
```

Replace with:

```cpp
    void appendAssistantDelta(const QString &delta);
    void flushHoldBuffer();
```

- [ ] **Step 4: Update ChatController.cpp**

Replace the existing `appendAssistantDelta` definition (around line 253-269) with:

```cpp
void ChatController::appendAssistantDelta(const QString &delta)
{
    if (m_cancelled) {
        return;
    }

    m_holdBuffer.append(delta);
    flushHoldBuffer();
}

void ChatController::flushHoldBuffer()
{
    if (m_cancelled || m_holdBuffer.isEmpty()) {
        return;
    }

    if (m_assistantMessageIndex < 0 || m_assistantMessageIndex >= m_messages.size()) {
        appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
        m_assistantMessageIndex = m_messages.size() - 1;
    }

    QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
    message.insert(QStringLiteral("text"), message.value(QStringLiteral("text")).toString() + m_holdBuffer);
    message.insert(QStringLiteral("pending"), true);
    m_messages[m_assistantMessageIndex] = message;
    m_holdBuffer.clear();
    emit messagesChanged();
}
```

Also, in `cancelCurrentReply` (around line 156-181), clear the buffer too. Find:

```cpp
    m_cancelled = true;
    if (m_currentReply) {
```

Insert right after `m_cancelled = true;`:

```cpp
    m_holdBuffer.clear();
```

And in `finishCurrentReply` (around line 344-357), after the `m_assistantMessageIndex = -1;` line, add `m_holdBuffer.clear();`. Find:

```cpp
    m_assistantMessageIndex = -1;
    m_currentReply.clear();
    setSending(false);
```

Replace with:

```cpp
    m_assistantMessageIndex = -1;
    m_holdBuffer.clear();
    m_currentReply.clear();
    setSending(false);
```

- [ ] **Step 5: Run build + smoke + contract**

Run:

```bash
cmake --build build --target chat_controller_smoke && \
ctest --test-dir build -R chat_controller_smoke --output-on-failure && \
python3 tests/check_phase_2_1_provider.py
```

Expected: smoke passes, contract still fails on later assertions (expressions in body, hold buffer in smoke). The `m_holdBuffer` / `flushHoldBuffer` / `hold buffer` assertions should now pass.

- [ ] **Step 6: Commit**

```bash
git add apps/desktop/src/chat/ChatController.h apps/desktop/src/chat/ChatController.cpp apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "feat(chat): ChatController hold-buffer plumbing"
```

## Task 11: Qt ChatController — Send Expressions in Request Body

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.cpp`

- [ ] **Step 1: Update sendMessage to embed expressions**

In `apps/desktop/src/chat/ChatController.cpp`, locate the `sendMessage` body that constructs the JSON body (around line 112-114). The current code is:

```cpp
    QJsonObject body;
    body.insert(QStringLiteral("conversationId"), QStringLiteral("default"));
    body.insert(QStringLiteral("message"), trimmed);
```

Add includes near the top of the file (after existing `<QJsonObject>` include):

```cpp
#include <QJsonArray>
```

Then `#include "pet/manifest/SkinManifest.h"` if it isn't already pulled in via `PetRuntime.h`. (Check `#include "pet/PetRuntime.h"` — if it transitively exposes `SkinManifest`, no new include is needed. If compile fails later, add the include then.)

Replace the body construction with:

```cpp
    QJsonObject body;
    body.insert(QStringLiteral("conversationId"), QStringLiteral("default"));
    body.insert(QStringLiteral("message"), trimmed);

    if (m_runtime != nullptr) {
        QJsonArray expressionsArray;
        const auto &manifestExpressions = m_runtime->manifest().expressions;
        for (auto it = manifestExpressions.constBegin(); it != manifestExpressions.constEnd(); ++it) {
            const auto &def = it.value();
            QJsonObject entry;
            entry.insert(QStringLiteral("id"), def.id);
            if (!def.label.isEmpty()) {
                entry.insert(QStringLiteral("label"), def.label);
            }
            if (!def.description.isEmpty()) {
                entry.insert(QStringLiteral("description"), def.description);
            }
            if (!def.allowedStates.isEmpty()) {
                QJsonArray allowed;
                for (const QString &state : def.allowedStates) {
                    allowed.append(state);
                }
                entry.insert(QStringLiteral("allowedStates"), allowed);
            }
            expressionsArray.append(entry);
        }
        body.insert(QStringLiteral("expressions"), expressionsArray);
    }
```

- [ ] **Step 2: Run contract check**

Run:

```bash
python3 tests/check_phase_2_1_provider.py
```

Expected: `"expressions"` assertion now passes; check still fails on missing 阶段记录 doc.

- [ ] **Step 3: Build and run smoke**

Run:

```bash
cmake --build build --target chat_controller_smoke && \
ctest --test-dir build -R "chat_controller_smoke|chat_stream_event_parser_smoke" --output-on-failure
```

Expected: green.

- [ ] **Step 4: Commit**

```bash
git add apps/desktop/src/chat/ChatController.cpp
git commit -m "feat(chat): send manifest expressions in chat request body"
```

## Task 12: Stage Record + Doc Index

**Files:**
- Create: `docs/v2/阶段记录/Phase 2.1 OpenAI-compatible Provider.md`
- Modify: `docs/v2/文档索引.md`

- [ ] **Step 1: Write the stage record**

Create `docs/v2/阶段记录/Phase 2.1 OpenAI-compatible Provider.md`:

```markdown
# Phase 2.1 OpenAI-compatible Provider

本文记录 Phase 2.1 的实现范围、验收方式和后续限制。Phase 2.1 接入真实 OpenAI-compatible Chat Completions provider，引入 `[EXPR:tag]` 标记协议骨架，并为 Phase 2.3 的完整状态机准备 hold buffer 雏形。

参考设计：`docs/v2/设计方案/AI 聊天动画编排设计.md` §11 Phase 2.1 行。

## 完成范围

- 新增 `internal/chat/config`：从 `MILES_PROVIDER_BASE_URL` / `MILES_PROVIDER_API_KEY` / `MILES_PROVIDER_MODEL` / `MILES_PROVIDER_TEMPERATURE` / `MILES_PROVIDER_MAX_TOKENS` 环境变量读取 provider 配置；`Enabled()` 判断是否齐备。
- 新增 `internal/chat/expression`：`[EXPR:tag]` 流式 token 解析器，支持跨 chunk 边界、未知 tag 降级到 `neutral`、`Flush` 时残留作为 text 输出。
- 新增 `internal/chat/openai`：OpenAI-compatible provider 适配器，POST `/v1/chat/completions` with stream=true，构建带 expression 清单的 system prompt，把 upstream SSE delta 通过 expression parser 转换为我们的 AG-UI envelope。
- `main.go` 根据 `config.Enabled()` 选择 provider：齐备时是 openai-compatible，否则 fallback 到 mock 并在 `/health` 标 `provider: "mock-fallback"`。
- `Request` 结构增加显式 JSON tag 和 `Expressions []ExpressionInfo` 字段。
- `api.Server` 接受 provider label，加 `http.MaxBytesReader`（1MB 限制），共享常量 `DefaultListenAddr = "127.0.0.1:39710"`。
- Qt `ChatController` 在 `sendMessage` 时携带当前皮肤 `manifest.expressions`。
- Qt `ChatController` 新增 `m_holdBuffer` / `flushHoldBuffer`：所有 `TEXT_MESSAGE_CONTENT` 通过 hold buffer 入 UI，当前实现为"立即 flush"，为 Phase 2.3 状态机预留位置。

## 验收命令

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
python3 tests/check_phase_2_1_provider.py
(cd apps/agent-core && go test ./...)
cmake --build build --target MilesEdgeworthDesktop
ctest --test-dir build -R "chat_stream_event_parser_smoke|chat_controller_smoke|check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider" --output-on-failure
```

## 当前限制

- ChatController 状态机仍是"流过即显示"，没有按 expression 切换 gate 文字。Phase 2.3 落地完整状态机和字符速率限制器。
- 配置只能通过环境变量；图形化设置面板和 Keychain 在 Phase 2.2。
- 会话历史不持久化；persona prompt 是最小版本。
- expression marker 解析放在 sidecar，Qt 仍然只接收已解析的事件。
- Provider 端口仍硬编码在 `DefaultListenAddr` / `ChatController.cpp`，未来如需动态端口由 sidecar 通过 stdout 上报。

## 后续入口

- Phase 2.2：设置 UI + API Key 安全存储。
- Phase 2.3：会话历史、完整 persona prompt、ChatController 状态机、字符速率限制器。
- Phase 2.4：Phased 动画与 `requestCleanFinishAndNotify` / `requestBoundaryAndNotify`。
```

- [ ] **Step 2: Update doc index**

In `docs/v2/文档索引.md`, find the last entry under "## 阶段记录":

```markdown
- [Phase 2.0 AI Chat MVP 骨架](阶段记录/Phase%202.0%20AI%20Chat%20MVP%20骨架.md)：...
```

Append after that line:

```markdown
- [Phase 2.1 OpenAI-compatible Provider](阶段记录/Phase%202.1%20OpenAI-compatible%20Provider.md)：Phase 2.1 的 OpenAI-compatible provider 接入、`[EXPR:tag]` 标记协议骨架、`Request` JSON tag 升级、hold buffer 雏形和服务端 hardening。
```

- [ ] **Step 3: Run final contract check**

Run:

```bash
python3 tests/check_phase_2_1_provider.py
```

Expected: `phase 2.1 provider contract ok`.

- [ ] **Step 4: Commit**

```bash
git add docs/v2/阶段记录/ docs/v2/文档索引.md
git commit -m "docs: phase 2.1 stage record + index"
```

## Task 13: Full Verification Pass

**Files:** (no edits — verification only)

- [ ] **Step 1: Run the full test matrix**

Run:

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py && \
python3 tests/check_phase_2_1_provider.py && \
(cd apps/agent-core && go test ./...) && \
cmake --build build --target MilesEdgeworthDesktop && \
ctest --test-dir build -R "chat_stream_event_parser_smoke|chat_controller_smoke|check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider" --output-on-failure
```

Expected:
- Both contract checks print `... ok`.
- Go tests: all packages green.
- CMake build: succeeds.
- CTest: 4 tests pass.

- [ ] **Step 2: Manual smoke against mock fallback**

In one terminal:

```bash
./build/apps/agent-core/miles-agent -addr 127.0.0.1:39710
```

Expected stderr: `provider: mock-fallback ...` and `miles-agent listening on 127.0.0.1:39710`.

In another terminal:

```bash
curl -s http://127.0.0.1:39710/health
```

Expected JSON contains `"provider":"mock-fallback"`.

```bash
curl -N -X POST http://127.0.0.1:39710/v1/chat/messages \
  -H 'Content-Type: application/json' \
  -d '{"conversationId":"d","message":"hi","expressions":[{"id":"neutral"},{"id":"objection"}]}'
```

Expected: SSE stream of AG-UI events, ending with `RUN_FINISHED`. `Ctrl-C` to exit; kill sidecar.

- [ ] **Step 3: Manual smoke against real provider (optional, requires API key)**

```bash
MILES_PROVIDER_BASE_URL=https://api.deepseek.com \
MILES_PROVIDER_API_KEY=sk-... \
MILES_PROVIDER_MODEL=deepseek-chat \
./build/apps/agent-core/miles-agent
```

Then `curl` as above. Expected: stream comes from real provider; observe `[EXPR:...]` style markers being converted into `CUSTOM` events.

If you don't have credentials, skip this step.

- [ ] **Step 4: No commit**

This task only verifies prior commits.

## Self-Review Notes

After implementing all tasks, sanity-check:

1. **Spec coverage:**
   - Real OpenAI-compatible provider → Tasks 6–8.
   - `[EXPR:x]` parser → Tasks 3–4.
   - Provider config from env → Task 2.
   - Mock fallback → Task 9.
   - Expressions in request body → Task 11.
   - Hold buffer plumbing → Task 10.
   - Server hardening → Task 9 step 4.
   - Stage record + index → Task 12.
   - Verification → Task 13.

2. **Type consistency:**
   - `chat.ExpressionInfo` fields match JSON keys used in Qt (`id`, `label`, `description`, `allowedStates`).
   - `api.NewServer(provider, label)` signature consistent across `main.go` and tests.
   - `expression.Parser` field names (`KnownTags`, `FallbackTag`, `OnText`, `OnExpression`) match test usage.

3. **Out-of-band work not in this plan:**
   - Update `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md` to reference `AI 聊天动画编排设计.md` from each sub-phase row. This is a documentation-only edit, done separately from the implementation plan.
