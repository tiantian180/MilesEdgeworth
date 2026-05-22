package service

import (
	"context"
	"errors"
	"strings"
	"testing"

	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/store"
)

type fakeProvider struct {
	completeText  string
	completeTexts []string
	completeErr   error
	completeCalls int
	onComplete    func()
}

func (p *fakeProvider) StreamChat(context.Context, chat.ChatParams) (<-chan chat.StreamEvent, error) {
	panic("not used")
}

func (p *fakeProvider) Complete(ctx context.Context, params chat.ChatParams) (string, error) {
	_ = ctx
	_ = params
	p.completeCalls++
	if p.onComplete != nil {
		p.onComplete()
	}
	if len(p.completeTexts) >= p.completeCalls {
		return p.completeTexts[p.completeCalls-1], p.completeErr
	}
	return p.completeText, p.completeErr
}

type fakeCatalog struct {
	window int
}

func (c fakeCatalog) ContextWindow(string) int {
	return c.window
}

func openTestStore(t *testing.T) *store.Store {
	t.Helper()
	s, err := store.Open(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = s.Close() })
	return s
}

func newTestService(s *store.Store, p chat.Provider, window int) *Service {
	return New(s, p, fakeCatalog{window: window}, "test-model")
}

func appendMessage(t *testing.T, s *store.Store, conversationID, role, content string) store.Message {
	t.Helper()
	msg, err := s.AppendMessage(conversationID, role, content, false)
	if err != nil {
		t.Fatal(err)
	}
	return msg
}

func TestEstimateTokensUsesRuneCount(t *testing.T) {
	if got := EstimateTokens("你好"); got != 4 {
		t.Fatalf("EstimateTokens = %d, want 4", got)
	}
}

func TestBuildMessagesPutsPersonaBeforeExpressionRules(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	appendMessage(t, s, conv.ID, store.RoleUser, "指出矛盾。")

	messages, known, err := newTestService(s, &fakeProvider{}, 8192).BuildMessages(context.Background(), BuildRequest{
		ConversationID: conv.ID,
		PersonaPrompt:  "成步堂，证言中的矛盾太明显了。",
		Expressions: []chat.ExpressionInfo{
			{ID: "objection", Label: "强烈反驳"},
		},
	})
	if err != nil {
		t.Fatal(err)
	}

	if len(messages) < 2 {
		t.Fatalf("messages = %+v, want system plus history", messages)
	}
	if messages[0].Role != "system" {
		t.Fatalf("first role = %q, want system", messages[0].Role)
	}
	system := messages[0].Content
	if !strings.HasPrefix(system, "成步堂，证言中的矛盾太明显了。") {
		t.Fatalf("system prompt = %q, want persona first", system)
	}
	for _, want := range []string{
		"回复时在每段文字开头用 [EXPR:id] 标记当前表达。",
		"只能使用方括号中列出的 id 原文，不要翻译 id，也不要使用中文 label。",
		"例如使用 [EXPR:objection]，不要输出 [EXPR:异议]。",
		"回复的第一段文字必须有标记。",
		"当前可用表达标签：",
		"- objection：强烈反驳",
	} {
		if !strings.Contains(system, want) {
			t.Fatalf("system prompt missing %q:\n%s", want, system)
		}
	}
	if strings.Contains(system, "[EXPR:强烈反驳]") {
		t.Fatalf("system prompt translated expression id:\n%s", system)
	}
	if len(known) != 1 || known[0] != "objection" {
		t.Fatalf("known ids = %+v, want objection", known)
	}
}

func TestBuildMessagesInjectsSummaryIntoSystemPrompt(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	old := appendMessage(t, s, conv.ID, store.RoleUser, "旧证言")
	if err := s.ReplaceSummary(conv.ID, old.ID, "用户已经提交过一份证言。"); err != nil {
		t.Fatal(err)
	}
	appendMessage(t, s, conv.ID, store.RoleUser, "继续询问。")

	messages, _, err := newTestService(s, &fakeProvider{}, 8192).BuildMessages(context.Background(), BuildRequest{
		ConversationID: conv.ID,
	})
	if err != nil {
		t.Fatal(err)
	}

	system := messages[0].Content
	if !strings.Contains(system, "以下是较早对话的摘要：\n用户已经提交过一份证言。") {
		t.Fatalf("summary not injected into system prompt:\n%s", system)
	}
	if !strings.Contains(system, "回复时在每段文字开头用 [EXPR:id] 标记当前表达。") {
		t.Fatalf("system prompt missing expression format instructions:\n%s", system)
	}
	if strings.Contains(system, "当前可用表达标签：") {
		t.Fatalf("empty expressions should omit expression list:\n%s", system)
	}
	for _, msg := range messages[1:] {
		if strings.Contains(msg.Content, "用户已经提交过一份证言。") {
			t.Fatalf("summary appeared outside system prompt: %+v", messages)
		}
	}
}

func TestBuildMessagesEmitsSummarizingBeforeSummaryCall(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	appendMessage(t, s, conv.ID, store.RoleUser, strings.Repeat("旧", 700))
	appendMessage(t, s, conv.ID, store.RoleAssistant, "记录。")
	appendMessage(t, s, conv.ID, store.RoleUser, "最近问题。")
	appendMessage(t, s, conv.ID, store.RoleAssistant, "最近回答。")
	appendMessage(t, s, conv.ID, store.RoleUser, "当前问题。")

	var order []string
	provider := &fakeProvider{
		completeText: "压缩后的记忆",
		onComplete: func() {
			order = append(order, "complete")
		},
	}
	_, _, err = newTestService(s, provider, 2000).BuildMessages(context.Background(), BuildRequest{
		ConversationID: conv.ID,
		RunID:          "run-1",
		Emit: func(event chat.StreamEvent) bool {
			order = append(order, event.Name)
			if event.Type != "CUSTOM" || event.Name != SummarizingEventName || event.RunID != "run-1" {
				t.Fatalf("summarizing event = %+v", event)
			}
			return true
		},
	})
	if err != nil {
		t.Fatal(err)
	}

	if provider.completeCalls != 1 {
		t.Fatalf("completeCalls = %d, want 1", provider.completeCalls)
	}
	if strings.Join(order, ",") != SummarizingEventName+",complete" {
		t.Fatalf("call order = %+v, want event before complete", order)
	}
}

func TestBuildMessagesSummaryReplaceExcludesCurrentAndLatestTurn(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	oldUser := appendMessage(t, s, conv.ID, store.RoleUser, strings.Repeat("旧", 700))
	oldReply := appendMessage(t, s, conv.ID, store.RoleAssistant, "旧记录。")
	latestUser := appendMessage(t, s, conv.ID, store.RoleUser, "最近问题。")
	latestReply := appendMessage(t, s, conv.ID, store.RoleAssistant, "最近回答。")
	currentUser := appendMessage(t, s, conv.ID, store.RoleUser, "当前问题。")

	messages, _, err := newTestService(s, &fakeProvider{completeText: "压缩后的记忆"}, 2000).BuildMessages(context.Background(), BuildRequest{
		ConversationID: conv.ID,
	})
	if err != nil {
		t.Fatal(err)
	}

	stored, err := s.GetMessages(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if len(stored) != 4 {
		t.Fatalf("stored messages = %+v, want summary plus three protected messages", stored)
	}
	seenSummary := false
	seenProtected := map[int64]bool{}
	for _, msg := range stored {
		if msg.Role == store.RoleSummary && msg.Content == "压缩后的记忆" {
			seenSummary = true
			continue
		}
		seenProtected[msg.ID] = true
		if msg.ID == oldUser.ID || msg.ID == oldReply.ID {
			t.Fatalf("old eligible message was not replaced: %+v", stored)
		}
	}
	if !seenSummary {
		t.Fatalf("stored messages missing summary: %+v", stored)
	}
	if !seenProtected[latestUser.ID] || !seenProtected[latestReply.ID] || !seenProtected[currentUser.ID] {
		t.Fatalf("protected messages not preserved: stored=%+v", stored)
	}
	system := messages[0].Content
	if !strings.Contains(system, "以下是较早对话的摘要：\n压缩后的记忆") {
		t.Fatalf("rebuilt system prompt missing new summary:\n%s", system)
	}
	if messages[len(messages)-1].Content != "当前问题。" {
		t.Fatalf("last message = %+v, want current user preserved", messages[len(messages)-1])
	}
}

func TestBuildMessagesUsesOnlyLatestSummaryAfterTwoSummaryCycles(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	appendMessage(t, s, conv.ID, store.RoleUser, strings.Repeat("早期", 350))
	appendMessage(t, s, conv.ID, store.RoleAssistant, "早期回答。")
	appendMessage(t, s, conv.ID, store.RoleUser, strings.Repeat("第一轮最近问题", 80))
	appendMessage(t, s, conv.ID, store.RoleAssistant, strings.Repeat("第一轮最近回答", 80))
	appendMessage(t, s, conv.ID, store.RoleUser, strings.Repeat("第一轮当前问题", 80))

	provider := &fakeProvider{completeTexts: []string{"旧完整摘要", "新完整摘要"}}
	service := newTestService(s, provider, 2000)
	if _, _, err := service.BuildMessages(context.Background(), BuildRequest{ConversationID: conv.ID}); err != nil {
		t.Fatal(err)
	}

	appendMessage(t, s, conv.ID, store.RoleAssistant, strings.Repeat("第一轮当前回答", 80))
	appendMessage(t, s, conv.ID, store.RoleUser, "第二轮当前问题。")
	messages, _, err := service.BuildMessages(context.Background(), BuildRequest{ConversationID: conv.ID})
	if err != nil {
		t.Fatal(err)
	}

	if provider.completeCalls != 2 {
		t.Fatalf("completeCalls = %d, want 2", provider.completeCalls)
	}
	system := messages[0].Content
	if !strings.Contains(system, "以下是较早对话的摘要：\n新完整摘要") {
		t.Fatalf("system prompt missing latest summary:\n%s", system)
	}
	if strings.Contains(system, "旧完整摘要") {
		t.Fatalf("system prompt included stale summary:\n%s", system)
	}
	if messages[len(messages)-1].Role != store.RoleUser || messages[len(messages)-1].Content != "第二轮当前问题。" {
		t.Fatalf("current user not preserved: %+v", messages[len(messages)-1])
	}
}

func TestBuildMessagesFallsBackToTruncationWhenSummaryFails(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	appendMessage(t, s, conv.ID, store.RoleUser, strings.Repeat("旧", 700))
	appendMessage(t, s, conv.ID, store.RoleAssistant, "旧记录。")
	appendMessage(t, s, conv.ID, store.RoleUser, "最近问题。")
	appendMessage(t, s, conv.ID, store.RoleAssistant, "最近回答。")
	currentUser := appendMessage(t, s, conv.ID, store.RoleUser, "当前问题。")

	messages, _, err := newTestService(s, &fakeProvider{completeErr: errors.New("summary failed")}, 2000).BuildMessages(context.Background(), BuildRequest{
		ConversationID: conv.ID,
	})
	if err != nil {
		t.Fatal(err)
	}

	for _, msg := range messages {
		if strings.Contains(msg.Content, strings.Repeat("旧", 20)) {
			t.Fatalf("old eligible message was not truncated: %+v", messages)
		}
	}
	if messages[len(messages)-1].Role != store.RoleUser || messages[len(messages)-1].Content != currentUser.Content {
		t.Fatalf("current user not preserved: %+v", messages)
	}
	stored, err := s.GetMessages(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if len(stored) != 5 {
		t.Fatalf("fallback should not mutate store, got %+v", stored)
	}
}
