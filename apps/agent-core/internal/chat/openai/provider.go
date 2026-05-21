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
	return buildSystemPrompt(expressions)
}

func buildSystemPrompt(expressions []chat.ExpressionInfo) string {
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
