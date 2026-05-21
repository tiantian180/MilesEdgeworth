package expression_test

import (
	"strings"
	"testing"

	"milesedgeworth/agent-core/internal/chat/expression"
)

type capture struct {
	out strings.Builder
}

func (c *capture) text(s string)   { c.out.WriteString("T(" + s + ")") }
func (c *capture) expr(tag string) { c.out.WriteString("E(" + tag + ")") }
func (c *capture) string() string  { return c.out.String() }

func newParser(c *capture, known []string) *expression.Parser {
	return &expression.Parser{
		KnownTags:    known,
		FallbackTag:  "neutral",
		OnText:       c.text,
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
