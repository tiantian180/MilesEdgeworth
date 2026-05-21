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
