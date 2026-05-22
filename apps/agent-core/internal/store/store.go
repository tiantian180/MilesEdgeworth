package store

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	_ "modernc.org/sqlite"
)

const (
	RoleUser      = "user"
	RoleAssistant = "assistant"
	RoleSummary   = "summary"
)

type Store struct {
	db *sql.DB
}

type Conversation struct {
	ID        string `json:"id"`
	Title     string `json:"title"`
	SkinID    string `json:"skinId"`
	CreatedAt string `json:"createdAt"`
	UpdatedAt string `json:"updatedAt"`
}

type Message struct {
	ID             int64  `json:"id"`
	ConversationID string `json:"conversationId"`
	Role           string `json:"role"`
	Content        string `json:"content"`
	IsPartial      bool   `json:"isPartial"`
	CreatedAt      string `json:"createdAt"`
}

var (
	ErrNotFound    = errors.New("not found")
	ErrInvalidRole = errors.New("invalid role")
)

const schema = `
CREATE TABLE IF NOT EXISTS conversations (
    id          TEXT PRIMARY KEY,
    title       TEXT NOT NULL DEFAULT '',
    skin_id     TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    updated_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE TABLE IF NOT EXISTS messages (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    conversation_id TEXT NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    role            TEXT NOT NULL,
    content         TEXT NOT NULL,
    is_partial      INTEGER NOT NULL DEFAULT 0,
    created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_messages_conversation ON messages(conversation_id, id);
`

func Open(dataDir string) (*Store, error) {
	if err := os.MkdirAll(dataDir, 0700); err != nil {
		return nil, fmt.Errorf("create data dir: %w", err)
	}

	db, err := sql.Open("sqlite", filepath.Join(dataDir, "chat.db"))
	if err != nil {
		return nil, fmt.Errorf("open sqlite: %w", err)
	}
	db.SetMaxOpenConns(1)
	db.SetMaxIdleConns(1)

	if _, err := db.Exec(`PRAGMA foreign_keys = ON`); err != nil {
		_ = db.Close()
		return nil, fmt.Errorf("enable foreign keys: %w", err)
	}
	if _, err := db.Exec(schema); err != nil {
		_ = db.Close()
		return nil, fmt.Errorf("initialize schema: %w", err)
	}

	return &Store{db: db}, nil
}

func (s *Store) Close() error {
	return s.db.Close()
}

func newUUIDV4() (string, error) {
	var b [16]byte
	if _, err := rand.Read(b[:]); err != nil {
		return "", err
	}
	b[6] = (b[6] & 0x0f) | 0x40
	b[8] = (b[8] & 0x3f) | 0x80

	encoded := hex.EncodeToString(b[:])
	return strings.Join([]string{
		encoded[0:8],
		encoded[8:12],
		encoded[12:16],
		encoded[16:20],
		encoded[20:32],
	}, "-"), nil
}

func (s *Store) CreateConversation(skinID string) (Conversation, error) {
	id, err := newUUIDV4()
	if err != nil {
		return Conversation{}, fmt.Errorf("generate conversation id: %w", err)
	}
	if _, err := s.db.Exec(`INSERT INTO conversations (id, skin_id) VALUES (?, ?)`, id, skinID); err != nil {
		return Conversation{}, fmt.Errorf("insert conversation: %w", err)
	}
	return s.GetConversation(id)
}

func (s *Store) ListConversations(limit int) ([]Conversation, error) {
	if limit <= 0 {
		limit = 100
	}
	rows, err := s.db.Query(`
		SELECT id, title, skin_id, created_at, updated_at
		FROM conversations
		ORDER BY updated_at DESC, id DESC
		LIMIT ?
	`, limit)
	if err != nil {
		return nil, fmt.Errorf("list conversations: %w", err)
	}
	defer rows.Close()

	var conversations []Conversation
	for rows.Next() {
		var conv Conversation
		if err := scanConversation(rows, &conv); err != nil {
			return nil, err
		}
		conversations = append(conversations, conv)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("list conversations: %w", err)
	}
	return conversations, nil
}

func (s *Store) GetConversation(id string) (Conversation, error) {
	var conv Conversation
	err := scanConversation(s.db.QueryRow(`
		SELECT id, title, skin_id, created_at, updated_at
		FROM conversations
		WHERE id = ?
	`, id), &conv)
	if errors.Is(err, sql.ErrNoRows) {
		return Conversation{}, ErrNotFound
	}
	if err != nil {
		return Conversation{}, err
	}
	return conv, nil
}

func (s *Store) DeleteConversation(id string) error {
	if _, err := s.db.Exec(`DELETE FROM conversations WHERE id = ?`, id); err != nil {
		return fmt.Errorf("delete conversation: %w", err)
	}
	return nil
}

func (s *Store) UpdateConversationTitle(id, title string) error {
	result, err := s.db.Exec(`
		UPDATE conversations
		SET title = ?, updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
		WHERE id = ?
	`, title, id)
	if err != nil {
		return fmt.Errorf("update conversation title: %w", err)
	}
	rowsAffected, err := result.RowsAffected()
	if err != nil {
		return fmt.Errorf("update conversation title: %w", err)
	}
	if rowsAffected == 0 {
		return ErrNotFound
	}
	return nil
}

func (s *Store) AppendMessage(conversationID, role, content string, partial bool) (Message, error) {
	if err := validateRole(role); err != nil {
		return Message{}, err
	}
	if role != RoleAssistant {
		partial = false
	}

	tx, err := s.db.BeginTx(context.Background(), nil)
	if err != nil {
		return Message{}, fmt.Errorf("begin append message: %w", err)
	}
	committed := false
	defer func() {
		if !committed {
			_ = tx.Rollback()
		}
	}()

	var title string
	if err := tx.QueryRow(`SELECT title FROM conversations WHERE id = ?`, conversationID).Scan(&title); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return Message{}, ErrNotFound
		}
		return Message{}, fmt.Errorf("load conversation: %w", err)
	}

	shouldSetTitle := false
	if role == RoleUser && title == "" {
		var priorUserMessages int
		if err := tx.QueryRow(`
			SELECT COUNT(*)
			FROM messages
			WHERE conversation_id = ? AND role = ?
		`, conversationID, RoleUser).Scan(&priorUserMessages); err != nil {
			return Message{}, fmt.Errorf("count user messages: %w", err)
		}
		shouldSetTitle = priorUserMessages == 0
	}

	var msg Message
	partialValue := boolToInt(partial)
	err = scanMessage(tx.QueryRow(`
		INSERT INTO messages (conversation_id, role, content, is_partial)
		VALUES (?, ?, ?, ?)
		RETURNING id, conversation_id, role, content, is_partial, created_at
	`, conversationID, role, content, partialValue), &msg)
	if err != nil {
		return Message{}, fmt.Errorf("insert message: %w", err)
	}

	newTitle := title
	if shouldSetTitle {
		newTitle = firstRunes(content, 30)
	}
	if _, err := tx.Exec(`
		UPDATE conversations
		SET title = ?, updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
		WHERE id = ?
	`, newTitle, conversationID); err != nil {
		return Message{}, fmt.Errorf("touch conversation: %w", err)
	}

	if err := tx.Commit(); err != nil {
		return Message{}, fmt.Errorf("commit append message: %w", err)
	}
	committed = true
	return msg, nil
}

func (s *Store) GetMessages(conversationID string) ([]Message, error) {
	rows, err := s.db.Query(`
		SELECT id, conversation_id, role, content, is_partial, created_at
		FROM messages
		WHERE conversation_id = ?
		ORDER BY id ASC
	`, conversationID)
	if err != nil {
		return nil, fmt.Errorf("get messages: %w", err)
	}
	defer rows.Close()

	var messages []Message
	for rows.Next() {
		var msg Message
		if err := scanMessage(rows, &msg); err != nil {
			return nil, err
		}
		messages = append(messages, msg)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("get messages: %w", err)
	}
	return messages, nil
}

func (s *Store) ReplaceSummary(conversationID string, upToMessageID int64, summary string) error {
	tx, err := s.db.BeginTx(context.Background(), nil)
	if err != nil {
		return fmt.Errorf("begin replace summary: %w", err)
	}
	committed := false
	defer func() {
		if !committed {
			_ = tx.Rollback()
		}
	}()

	var exists int
	if err := tx.QueryRow(`SELECT 1 FROM conversations WHERE id = ?`, conversationID).Scan(&exists); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return ErrNotFound
		}
		return fmt.Errorf("load conversation: %w", err)
	}

	if _, err := tx.Exec(`
		DELETE FROM messages
		WHERE conversation_id = ? AND id <= ?
	`, conversationID, upToMessageID); err != nil {
		return fmt.Errorf("delete old messages: %w", err)
	}
	if _, err := tx.Exec(`
		INSERT INTO messages (conversation_id, role, content, is_partial)
		VALUES (?, ?, ?, 0)
	`, conversationID, RoleSummary, summary); err != nil {
		return fmt.Errorf("insert summary: %w", err)
	}
	if _, err := tx.Exec(`
		UPDATE conversations
		SET updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
		WHERE id = ?
	`, conversationID); err != nil {
		return fmt.Errorf("touch conversation: %w", err)
	}

	if err := tx.Commit(); err != nil {
		return fmt.Errorf("commit replace summary: %w", err)
	}
	committed = true
	return nil
}

type scanner interface {
	Scan(dest ...any) error
}

func scanConversation(row scanner, conv *Conversation) error {
	return row.Scan(&conv.ID, &conv.Title, &conv.SkinID, &conv.CreatedAt, &conv.UpdatedAt)
}

func scanMessage(row scanner, msg *Message) error {
	var partial int
	if err := row.Scan(&msg.ID, &msg.ConversationID, &msg.Role, &msg.Content, &partial, &msg.CreatedAt); err != nil {
		return err
	}
	msg.IsPartial = partial != 0
	return nil
}

func boolToInt(v bool) int {
	if v {
		return 1
	}
	return 0
}

func validateRole(role string) error {
	switch role {
	case RoleUser, RoleAssistant, RoleSummary:
		return nil
	default:
		return fmt.Errorf("%w: %s", ErrInvalidRole, role)
	}
}

func firstRunes(s string, limit int) string {
	runes := []rune(s)
	if len(runes) <= limit {
		return s
	}
	return string(runes[:limit])
}
