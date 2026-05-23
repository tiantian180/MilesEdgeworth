package service

import (
	"context"
	"errors"
	"fmt"
	"strings"

	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/store"
)

const SummarizingEventName = "miles.chat.memory.summarizing"
const MissingProviderMessage = "未配置模型，请在设置中填写 API Key 和模型信息"

type ModelCatalog interface {
	ContextWindow(modelID string) int
}

type Service struct {
	store    *store.Store
	provider chat.Provider
	catalog  ModelCatalog
	modelID  string
}

type BuildRequest struct {
	ConversationID string
	PersonaPrompt  string
	Expressions    []chat.ExpressionInfo
	RunID          string
	MessageID      string
	Emit           func(chat.StreamEvent) bool
}

func New(store *store.Store, provider chat.Provider, catalog ModelCatalog, modelID string) *Service {
	return &Service{
		store:    store,
		provider: provider,
		catalog:  catalog,
		modelID:  modelID,
	}
}

func EstimateTokens(text string) int {
	return len([]rune(text)) * 2
}

func (s *Service) BuildMessages(ctx context.Context, req BuildRequest) ([]chat.Message, []string, error) {
	rows, err := s.store.GetMessages(req.ConversationID)
	if err != nil {
		return nil, nil, err
	}

	messages, knownExpressionIDs := buildMessagesFromRows(req.PersonaPrompt, req.Expressions, rows)
	if withinBudget(messages, s.contextLimit()) {
		return messages, knownExpressionIDs, nil
	}

	summaries, normalRows := splitRows(rows)
	eligible := eligibleForCompression(normalRows)
	if len(eligible) == 0 {
		return messages, knownExpressionIDs, nil
	}

	if req.Emit != nil {
		if ok := req.Emit(chat.StreamEvent{
			Type:  "CUSTOM",
			Name:  SummarizingEventName,
			RunID: req.RunID,
			Value: map[string]any{},
		}); !ok {
			if err := ctx.Err(); err != nil {
				return nil, nil, err
			}
			return nil, nil, context.Canceled
		}
	}

	summary, err := s.summarize(ctx, req, summaries, eligible)
	if err != nil {
		if ctxErr := ctx.Err(); ctxErr != nil {
			return nil, nil, ctxErr
		}
		if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
			return nil, nil, err
		}
	} else if trimmedSummary := strings.TrimSpace(summary); trimmedSummary != "" {
		upToMessageID := eligible[len(eligible)-1].ID
		remainingRows := rowsAfter(normalRows, upToMessageID)
		candidate, _ := buildMessagesFromParts(req.PersonaPrompt, req.Expressions, []string{trimmedSummary}, remainingRows)
		if withinBudget(candidate, s.contextLimit()) {
			if err := s.store.ReplaceSummary(req.ConversationID, upToMessageID, trimmedSummary); err != nil {
				return nil, nil, err
			}
			reloaded, err := s.store.GetMessages(req.ConversationID)
			if err != nil {
				return nil, nil, err
			}
			messages, knownExpressionIDs = buildMessagesFromRows(req.PersonaPrompt, req.Expressions, reloaded)
			return messages, knownExpressionIDs, nil
		}
	}

	messages = fallbackTruncate(req.PersonaPrompt, req.Expressions, summaries, normalRows, s.contextLimit())
	return messages, knownExpressionIDs, nil
}

func (s *Service) StreamChat(ctx context.Context, req BuildRequest) (<-chan chat.StreamEvent, error) {
	if s.provider == nil {
		events := make(chan chat.StreamEvent, 1)
		events <- chat.StreamEvent{
			Type:      "RUN_ERROR",
			RunID:     req.RunID,
			MessageID: req.MessageID,
			Error:     MissingProviderMessage,
		}
		close(events)
		return events, nil
	}

	messages, knownExpressionIDs, err := s.BuildMessages(ctx, req)
	if err != nil {
		return nil, err
	}
	return s.provider.StreamChat(ctx, chat.ChatParams{
		ConversationID:     req.ConversationID,
		RunID:              req.RunID,
		MessageID:          req.MessageID,
		Operation:          "chat",
		Messages:           messages,
		KnownExpressionIDs: knownExpressionIDs,
	})
}

func (s *Service) contextLimit() int {
	budget := 8192
	if s.catalog != nil {
		budget = s.catalog.ContextWindow(s.modelID)
	}
	if budget <= 0 {
		budget = 8192
	}
	return int(float64(budget) * 0.6)
}

func (s *Service) summarize(ctx context.Context, req BuildRequest, summaries []string, rows []store.Message) (string, error) {
	existingSummary := strings.Join(summaries, "\n\n")
	newMessages := formatRows(rows)

	var systemPrompt string
	var userPrompt string
	if strings.TrimSpace(existingSummary) == "" {
		systemPrompt = "请用简洁的中文总结以下对话，保留关键信息、用户偏好和重要结论。总结应是第三人称叙述。"
		userPrompt = newMessages
	} else {
		systemPrompt = "请将以下新对话内容整合到已有摘要中，保留关键信息、用户偏好和重要结论。输出更新后的完整摘要。"
		userPrompt = "已有摘要：\n" + existingSummary + "\n\n新对话：\n" + newMessages
	}

	return s.provider.Complete(ctx, chat.ChatParams{
		ConversationID: req.ConversationID,
		RunID:          req.RunID,
		MessageID:      req.MessageID,
		Operation:      "summary",
		Messages: []chat.Message{
			{Role: "system", Content: systemPrompt},
			{Role: "user", Content: userPrompt},
		},
	})
}

func buildMessagesFromRows(persona string, expressions []chat.ExpressionInfo, rows []store.Message) ([]chat.Message, []string) {
	summaries, normalRows := splitRows(rows)
	messages := []chat.Message{
		{Role: "system", Content: buildSystemPrompt(persona, expressions, summaries)},
	}
	for _, row := range normalRows {
		messages = append(messages, chat.Message{
			Role:    row.Role,
			Content: row.Content,
		})
	}
	return messages, knownExpressionIDs(expressions)
}

func splitRows(rows []store.Message) ([]string, []store.Message) {
	var latestSummary string
	var normalRows []store.Message
	for _, row := range rows {
		if row.Role == store.RoleSummary {
			if summary := strings.TrimSpace(row.Content); summary != "" {
				latestSummary = summary
			}
			continue
		}
		normalRows = append(normalRows, row)
	}
	var summaries []string
	if latestSummary != "" {
		summaries = append(summaries, latestSummary)
	}
	return summaries, normalRows
}

func buildSystemPrompt(persona string, expressions []chat.ExpressionInfo, summaries []string) string {
	var parts []string
	if strings.TrimSpace(persona) != "" {
		parts = append(parts, strings.TrimSpace(persona))
	}
	parts = append(parts, buildExpressionRules(expressions))
	if len(summaries) > 0 {
		parts = append(parts, "以下是较早对话的摘要：\n"+strings.Join(summaries, "\n\n"))
	}
	return strings.Join(parts, "\n\n")
}

func buildExpressionRules(expressions []chat.ExpressionInfo) string {
	lines := []string{
		"回复时在每段文字开头用 [EXPR:id] 标记当前表达。",
		"只能使用方括号中列出的 id 原文，不要翻译 id，也不要使用中文 label。",
		"例如使用 [EXPR:objection]，不要输出 [EXPR:异议]。",
		"回复的第一段文字必须有标记。",
	}

	var items []string
	for _, expression := range expressions {
		id := strings.TrimSpace(expression.ID)
		if id == "" {
			continue
		}
		description := strings.TrimSpace(expression.Description)
		if description == "" {
			description = strings.TrimSpace(expression.Label)
		}
		if description == "" {
			items = append(items, "- "+id)
		} else {
			items = append(items, fmt.Sprintf("- %s：%s", id, description))
		}
	}
	if len(items) > 0 {
		lines = append(lines, "", "当前可用表达标签：")
		lines = append(lines, items...)
	}
	return strings.Join(lines, "\n")
}

func knownExpressionIDs(expressions []chat.ExpressionInfo) []string {
	ids := make([]string, 0, len(expressions))
	for _, expression := range expressions {
		id := strings.TrimSpace(expression.ID)
		if id != "" {
			ids = append(ids, id)
		}
	}
	return ids
}

func eligibleForCompression(rows []store.Message) []store.Message {
	protected := protectedIndexes(rows)
	eligible := make([]store.Message, 0, len(rows))
	for i, row := range rows {
		if !protected[i] {
			eligible = append(eligible, row)
		}
	}
	return eligible
}

func protectedIndexes(rows []store.Message) map[int]bool {
	protected := make(map[int]bool)
	if len(rows) == 0 {
		return protected
	}

	currentUserIndex := -1
	for i := len(rows) - 1; i >= 0; i-- {
		if rows[i].Role == store.RoleUser {
			currentUserIndex = i
			break
		}
	}
	if currentUserIndex == -1 {
		return protected
	}
	protected[currentUserIndex] = true

	latestTurnEnd := currentUserIndex - 1
	if latestTurnEnd < 0 {
		return protected
	}
	protected[latestTurnEnd] = true
	if rows[latestTurnEnd].Role != store.RoleAssistant {
		return protected
	}
	for i := latestTurnEnd - 1; i >= 0; i-- {
		if rows[i].Role == store.RoleUser {
			for j := i; j <= latestTurnEnd; j++ {
				protected[j] = true
			}
			break
		}
	}
	return protected
}

func fallbackTruncate(persona string, expressions []chat.ExpressionInfo, summaries []string, rows []store.Message, limit int) []chat.Message {
	protected := protectedIndexes(rows)
	kept := append([]store.Message(nil), rows...)
	for i := 0; i < len(rows); i++ {
		if protected[i] {
			continue
		}
		kept = removeMessageByID(kept, rows[i].ID)
		messages, _ := buildMessagesFromParts(persona, expressions, summaries, kept)
		if withinBudget(messages, limit) {
			return messages
		}
	}
	messages, _ := buildMessagesFromParts(persona, expressions, summaries, kept)
	return messages
}

func buildMessagesFromParts(persona string, expressions []chat.ExpressionInfo, summaries []string, rows []store.Message) ([]chat.Message, []string) {
	messages := []chat.Message{
		{Role: "system", Content: buildSystemPrompt(persona, expressions, summaries)},
	}
	for _, row := range rows {
		messages = append(messages, chat.Message{
			Role:    row.Role,
			Content: row.Content,
		})
	}
	return messages, knownExpressionIDs(expressions)
}

func removeMessageByID(rows []store.Message, id int64) []store.Message {
	out := rows[:0]
	for _, row := range rows {
		if row.ID != id {
			out = append(out, row)
		}
	}
	return out
}

func rowsAfter(rows []store.Message, messageID int64) []store.Message {
	out := make([]store.Message, 0, len(rows))
	for _, row := range rows {
		if row.ID > messageID {
			out = append(out, row)
		}
	}
	return out
}

func withinBudget(messages []chat.Message, limit int) bool {
	return estimateMessages(messages) <= limit
}

func estimateMessages(messages []chat.Message) int {
	total := 0
	for _, message := range messages {
		total += EstimateTokens(message.Content)
	}
	return total
}

func formatRows(rows []store.Message) string {
	lines := make([]string, 0, len(rows))
	for _, row := range rows {
		lines = append(lines, row.Role+": "+row.Content)
	}
	return strings.Join(lines, "\n")
}
