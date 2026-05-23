package store

import (
	"errors"
	"sort"
	"testing"
)

func openTestStore(t *testing.T) *Store {
	t.Helper()
	s, err := Open(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = s.Close() })
	return s
}

func TestStoreConversationLifecycle(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	if conv.ID == "" || conv.SkinID != "miles-edgeworth" {
		t.Fatalf("bad conversation: %+v", conv)
	}
	user, err := s.AppendMessage(conv.ID, RoleUser, "你好", false)
	if err != nil {
		t.Fatal(err)
	}
	assistant, err := s.AppendMessage(conv.ID, RoleAssistant, "异议。", false)
	if err != nil {
		t.Fatal(err)
	}
	messages, err := s.GetMessages(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if len(messages) != 2 || messages[0].ID != user.ID || messages[1].ID != assistant.ID {
		t.Fatalf("messages = %+v", messages)
	}
	updated, err := s.GetConversation(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if updated.Title != "你好" {
		t.Fatalf("title = %q, want first user message", updated.Title)
	}
}

func TestAppendMessageUpdatesConversationUpdatedAt(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	oldUpdatedAt := "2000-01-01T00:00:00.000Z"
	if _, err := s.db.Exec(`UPDATE conversations SET updated_at = ? WHERE id = ?`, oldUpdatedAt, conv.ID); err != nil {
		t.Fatal(err)
	}

	if _, err := s.AppendMessage(conv.ID, RoleUser, "这是一条更新会话时间的消息", false); err != nil {
		t.Fatal(err)
	}

	updated, err := s.GetConversation(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if updated.UpdatedAt == oldUpdatedAt {
		t.Fatalf("updated_at was not changed: %q", updated.UpdatedAt)
	}
}

func TestDeleteConversationCascadesMessages(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := s.AppendMessage(conv.ID, RoleUser, "证言开始。", false); err != nil {
		t.Fatal(err)
	}
	if _, err := s.AppendMessage(conv.ID, RoleAssistant, "继续。", false); err != nil {
		t.Fatal(err)
	}

	if err := s.DeleteConversation(conv.ID); err != nil {
		t.Fatal(err)
	}
	if _, err := s.GetConversation(conv.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("GetConversation error = %v, want ErrNotFound", err)
	}
	messages, err := s.GetMessages(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if len(messages) != 0 {
		t.Fatalf("messages after delete = %+v, want none", messages)
	}
}

func TestReplaceSummaryIsTransactional(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	first, err := s.AppendMessage(conv.ID, RoleUser, "第一句证言。", false)
	if err != nil {
		t.Fatal(err)
	}
	second, err := s.AppendMessage(conv.ID, RoleAssistant, "记录。", false)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := s.db.Exec(`
		CREATE TRIGGER fail_summary_insert
		BEFORE INSERT ON messages
		WHEN NEW.role = 'summary'
		BEGIN
			SELECT RAISE(ABORT, 'summary insert failed');
		END;
	`); err != nil {
		t.Fatal(err)
	}

	if err := s.ReplaceSummary(conv.ID, second.ID, "较早对话摘要"); err == nil {
		t.Fatal("ReplaceSummary succeeded, want trigger failure")
	}
	messages, err := s.GetMessages(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if len(messages) != 2 || messages[0].ID != first.ID || messages[1].ID != second.ID {
		t.Fatalf("messages after failed summary replace = %+v", messages)
	}

	if _, err := s.db.Exec(`DROP TRIGGER fail_summary_insert`); err != nil {
		t.Fatal(err)
	}
	if err := s.ReplaceSummary(conv.ID, second.ID, "较早对话摘要"); err != nil {
		t.Fatal(err)
	}
	messages, err = s.GetMessages(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if len(messages) != 1 || messages[0].Role != RoleSummary || messages[0].Content != "较早对话摘要" {
		t.Fatalf("messages after summary replace = %+v", messages)
	}
}

func TestReplaceSummaryDeletesExistingSummaryRows(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	first, err := s.AppendMessage(conv.ID, RoleUser, "第一段证言。", false)
	if err != nil {
		t.Fatal(err)
	}
	if err := s.ReplaceSummary(conv.ID, first.ID, "旧摘要"); err != nil {
		t.Fatal(err)
	}
	protected, err := s.AppendMessage(conv.ID, RoleUser, "保留的新消息。", false)
	if err != nil {
		t.Fatal(err)
	}
	if err := s.ReplaceSummary(conv.ID, first.ID, "新摘要"); err != nil {
		t.Fatal(err)
	}

	messages, err := s.GetMessages(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	var summaries []Message
	var keptProtected bool
	for _, msg := range messages {
		if msg.Role == RoleSummary {
			summaries = append(summaries, msg)
		}
		if msg.ID == protected.ID {
			keptProtected = true
		}
	}
	if len(summaries) != 1 || summaries[0].Content != "新摘要" {
		t.Fatalf("summaries = %+v, want only new summary; messages=%+v", summaries, messages)
	}
	if !keptProtected {
		t.Fatalf("protected message was deleted: %+v", messages)
	}
}

func TestUserAndSummaryCannotBePartial(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}
	user, err := s.AppendMessage(conv.ID, RoleUser, "用户消息不能是 partial", true)
	if err != nil {
		t.Fatal(err)
	}
	assistant, err := s.AppendMessage(conv.ID, RoleAssistant, "助手消息可以是 partial", true)
	if err != nil {
		t.Fatal(err)
	}
	if user.IsPartial {
		t.Fatalf("user IsPartial = true, want false")
	}
	if !assistant.IsPartial {
		t.Fatalf("assistant IsPartial = false, want true")
	}

	if err := s.ReplaceSummary(conv.ID, assistant.ID, "摘要不能是 partial"); err != nil {
		t.Fatal(err)
	}
	messages, err := s.GetMessages(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if len(messages) != 1 || messages[0].Role != RoleSummary || messages[0].IsPartial {
		t.Fatalf("summary messages = %+v, want one non-partial summary", messages)
	}
}

func TestAppendMessageRejectsInvalidRole(t *testing.T) {
	s := openTestStore(t)
	conv, err := s.CreateConversation("miles-edgeworth")
	if err != nil {
		t.Fatal(err)
	}

	if _, err := s.AppendMessage(conv.ID, "judge", "非法角色", false); !errors.Is(err, ErrInvalidRole) {
		t.Fatalf("AppendMessage error = %v, want invalid role error", err)
	}
	messages, err := s.GetMessages(conv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if len(messages) != 0 {
		t.Fatalf("messages = %+v, want none", messages)
	}
}

func TestReplaceSummaryMissingConversationReturnsErrNotFound(t *testing.T) {
	s := openTestStore(t)

	if err := s.ReplaceSummary("missing-conversation", 1, "摘要"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("ReplaceSummary error = %v, want ErrNotFound", err)
	}
}

func TestListConversationsHonorsLimitAndSortsByUpdatedAt(t *testing.T) {
	s := openTestStore(t)
	oldConv, err := s.CreateConversation("old")
	if err != nil {
		t.Fatal(err)
	}
	midConv, err := s.CreateConversation("mid")
	if err != nil {
		t.Fatal(err)
	}
	newConv, err := s.CreateConversation("new")
	if err != nil {
		t.Fatal(err)
	}
	updates := map[string]string{
		oldConv.ID: "2024-01-01T00:00:00.000Z",
		midConv.ID: "2024-01-02T00:00:00.000Z",
		newConv.ID: "2024-01-03T00:00:00.000Z",
	}
	for id, updatedAt := range updates {
		if _, err := s.db.Exec(`UPDATE conversations SET updated_at = ? WHERE id = ?`, updatedAt, id); err != nil {
			t.Fatal(err)
		}
	}

	conversations, err := s.ListConversations(2)
	if err != nil {
		t.Fatal(err)
	}
	if len(conversations) != 2 {
		t.Fatalf("len(conversations) = %d, want 2: %+v", len(conversations), conversations)
	}
	if conversations[0].ID != newConv.ID || conversations[1].ID != midConv.ID {
		t.Fatalf("conversation order = %+v, want new then mid", conversations)
	}
}

func TestListConversationsSortsTiesByIDDescending(t *testing.T) {
	s := openTestStore(t)
	ids := []string{"cccccccc-0000-4000-8000-000000000000", "aaaaaaaa-0000-4000-8000-000000000000", "bbbbbbbb-0000-4000-8000-000000000000"}
	for _, id := range ids {
		if _, err := s.db.Exec(`
			INSERT INTO conversations (id, skin_id, updated_at)
			VALUES (?, 'miles-edgeworth', '2024-01-01T00:00:00.000Z')
		`, id); err != nil {
			t.Fatal(err)
		}
	}
	sort.Sort(sort.Reverse(sort.StringSlice(ids)))

	conversations, err := s.ListConversations(0)
	if err != nil {
		t.Fatal(err)
	}
	if len(conversations) != len(ids) {
		t.Fatalf("len(conversations) = %d, want %d: %+v", len(conversations), len(ids), conversations)
	}
	for i, id := range ids {
		if conversations[i].ID != id {
			t.Fatalf("conversation order = %+v, want IDs desc %+v", conversations, ids)
		}
	}
}
