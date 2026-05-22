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

func TestParserAcceptsAllTagsWhenKnownListEmpty(t *testing.T) {
	cap := &capture{}
	p := newParser(cap, nil)
	p.Feed("[EXPR:objection]hi")
	p.Flush()
	if got := cap.string(); got != "E(objection)T(hi)" {
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
