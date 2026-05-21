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

// NewParser creates a streaming marker parser with the standard callback shape.
func NewParser(knownTags []string, fallbackTag string, onText func(string), onExpression func(string)) *Parser {
	return &Parser{
		KnownTags:    knownTags,
		FallbackTag:  fallbackTag,
		OnText:       onText,
		OnExpression: onExpression,
	}
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
