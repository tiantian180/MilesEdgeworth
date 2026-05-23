package api

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"net/http"
	"os"
	"strconv"
	"strings"

	"milesedgeworth/agent-core/internal/chat"
	chatservice "milesedgeworth/agent-core/internal/chat/service"
	"milesedgeworth/agent-core/internal/store"
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
	store         *store.Store
	chatService   *chatservice.Service
	providerLabel string
}

func NewServer(store *store.Store, chatService *chatservice.Service, providerLabel string) *Server {
	if providerLabel == "" {
		providerLabel = "unknown"
	}
	return &Server{store: store, chatService: chatService, providerLabel: providerLabel}
}

func (s *Server) Routes() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/health", s.handleHealth)
	mux.HandleFunc("/v1/conversations", s.handleConversations)
	mux.HandleFunc("/v1/conversations/", s.handleConversationByID)
	mux.HandleFunc("/v1/chat/messages", s.handleChatMessages)
	return mux
}

func (s *Server) handleHealth(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeMethodNotAllowed(w, http.MethodGet)
		return
	}

	writeJSON(w, http.StatusOK, map[string]any{
		"ok":       true,
		"pid":      os.Getpid(),
		"provider": s.providerLabel,
		"service":  "miles-agent",
	})
}

func (s *Server) handleConversations(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		limit := 100
		if rawLimit := strings.TrimSpace(r.URL.Query().Get("limit")); rawLimit != "" {
			parsed, err := strconv.Atoi(rawLimit)
			if err != nil {
				writeJSON(w, http.StatusBadRequest, map[string]any{"error": "invalid limit"})
				return
			}
			limit = parsed
		}

		conversations, err := s.store.ListConversations(limit)
		if err != nil {
			writeJSON(w, http.StatusInternalServerError, map[string]any{"error": err.Error()})
			return
		}
		writeJSON(w, http.StatusOK, conversationResponses(conversations))
	case http.MethodPost:
		r.Body = http.MaxBytesReader(w, r.Body, MaxRequestBodyBytes)
		var req struct {
			SkinID string `json:"skinId"`
		}
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			writeJSON(w, http.StatusBadRequest, map[string]any{"error": "invalid JSON"})
			return
		}
		req.SkinID = strings.TrimSpace(req.SkinID)
		if req.SkinID == "" {
			writeJSON(w, http.StatusBadRequest, map[string]any{"error": "skinId is required"})
			return
		}
		conv, err := s.store.CreateConversation(req.SkinID)
		if err != nil {
			writeJSON(w, http.StatusInternalServerError, map[string]any{"error": err.Error()})
			return
		}
		writeJSON(w, http.StatusCreated, newConversationResponse(conv))
	default:
		writeMethodNotAllowed(w, http.MethodGet, http.MethodPost)
	}
}

func (s *Server) handleConversationByID(w http.ResponseWriter, r *http.Request) {
	path := strings.TrimPrefix(r.URL.Path, "/v1/conversations/")
	if path == "" {
		http.NotFound(w, r)
		return
	}

	if strings.HasSuffix(path, "/messages") {
		id := strings.TrimSuffix(path, "/messages")
		if id == "" || strings.Contains(id, "/") {
			http.NotFound(w, r)
			return
		}
		s.handleConversationMessages(w, r, id)
		return
	}

	if strings.Contains(path, "/") {
		http.NotFound(w, r)
		return
	}
	if r.Method != http.MethodDelete {
		writeMethodNotAllowed(w, http.MethodDelete)
		return
	}
	if _, err := s.store.GetConversation(path); err != nil {
		writeStoreError(w, err)
		return
	}
	if err := s.store.DeleteConversation(path); err != nil {
		writeStoreError(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) handleConversationMessages(w http.ResponseWriter, r *http.Request, conversationID string) {
	if r.Method != http.MethodGet {
		writeMethodNotAllowed(w, http.MethodGet)
		return
	}
	if _, err := s.store.GetConversation(conversationID); err != nil {
		writeStoreError(w, err)
		return
	}
	messages, err := s.store.GetMessages(conversationID)
	if err != nil {
		writeStoreError(w, err)
		return
	}
	writeJSON(w, http.StatusOK, messageResponses(messages))
}

func (s *Server) handleChatMessages(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeMethodNotAllowed(w, http.MethodPost)
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
	req.ConversationID = strings.TrimSpace(req.ConversationID)
	if req.ConversationID == "" {
		writeJSON(w, http.StatusBadRequest, map[string]any{"error": "conversationId is required"})
		return
	}
	if _, err := s.store.GetConversation(req.ConversationID); err != nil {
		writeStoreError(w, err)
		return
	}
	if _, err := s.store.AppendMessage(req.ConversationID, store.RoleUser, req.Message, false); err != nil {
		writeStoreError(w, err)
		return
	}

	runID, err := newStreamID("run")
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]any{"error": err.Error()})
		return
	}
	messageID, err := newStreamID("msg")
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]any{"error": err.Error()})
		return
	}

	w.Header().Set("Content-Type", "text/event-stream; charset=utf-8")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.WriteHeader(http.StatusOK)

	controller := http.NewResponseController(w)
	ctx, cancel := context.WithCancel(r.Context())
	defer cancel()

	var reply strings.Builder
	partialPersisted := false
	persistPartialOnce := func(providerError bool) {
		if partialPersisted {
			return
		}
		if s.persistPartialReply(req.ConversationID, reply.String(), providerError) {
			partialPersisted = true
		}
	}
	writeEvent := func(event chat.StreamEvent) bool {
		if err := writeSSE(w, event); err != nil {
			cancel()
			return false
		}
		if err := controller.Flush(); err != nil {
			cancel()
			return false
		}
		return true
	}

	events, err := s.chatService.StreamChat(ctx, chatservice.BuildRequest{
		ConversationID: req.ConversationID,
		PersonaPrompt:  req.PersonaPrompt,
		Expressions:    req.Expressions,
		RunID:          runID,
		MessageID:      messageID,
		Emit:           writeEvent,
	})
	if err != nil {
		_ = writeEvent(chat.StreamEvent{Type: "RUN_ERROR", RunID: runID, Error: err.Error()})
		return
	}

	finished := false
	providerError := false
	for event := range events {
		if event.Type == "TEXT_MESSAGE_CONTENT" {
			reply.WriteString(event.Delta)
		}
		if event.Type == "RUN_ERROR" {
			providerError = true
		}
		if !writeEvent(event) {
			persistPartialOnce(providerError)
			return
		}
		if event.Type == "RUN_FINISHED" {
			finished = true
		}
	}

	content := reply.String()
	if providerError || content == "" {
		return
	}
	if finished && ctx.Err() == nil {
		_, _ = s.store.AppendMessage(req.ConversationID, store.RoleAssistant, content, false)
		return
	}
	persistPartialOnce(false)
}

func writeJSON(w http.ResponseWriter, status int, body any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(body)
}

func writeMethodNotAllowed(w http.ResponseWriter, methods ...string) {
	w.Header().Set("Allow", strings.Join(methods, ", "))
	writeJSON(w, http.StatusMethodNotAllowed, map[string]any{"error": "method not allowed"})
}

func writeSSE(w http.ResponseWriter, event chat.StreamEvent) error {
	body, err := json.Marshal(event)
	if err != nil {
		return err
	}
	_, err = w.Write(append(append([]byte("data: "), body...), '\n', '\n'))
	return err
}

func (s *Server) persistPartialReply(conversationID, content string, providerError bool) bool {
	if providerError || content == "" {
		return false
	}
	_, err := s.store.AppendMessage(conversationID, store.RoleAssistant, content, true)
	return err == nil
}

func writeStoreError(w http.ResponseWriter, err error) {
	if errors.Is(err, store.ErrNotFound) {
		writeJSON(w, http.StatusNotFound, map[string]any{"error": "conversation not found"})
		return
	}
	writeJSON(w, http.StatusInternalServerError, map[string]any{"error": err.Error()})
}

func newStreamID(prefix string) (string, error) {
	var b [8]byte
	if _, err := rand.Read(b[:]); err != nil {
		return "", err
	}
	return prefix + "-" + hex.EncodeToString(b[:]), nil
}

type conversationResponse struct {
	ID        string `json:"id"`
	Title     string `json:"title"`
	SkinID    string `json:"skinId"`
	UpdatedAt string `json:"updatedAt"`
}

func newConversationResponse(conv store.Conversation) conversationResponse {
	return conversationResponse{
		ID:        conv.ID,
		Title:     conv.Title,
		SkinID:    conv.SkinID,
		UpdatedAt: conv.UpdatedAt,
	}
}

func conversationResponses(conversations []store.Conversation) []conversationResponse {
	responses := make([]conversationResponse, 0, len(conversations))
	for _, conv := range conversations {
		responses = append(responses, newConversationResponse(conv))
	}
	return responses
}

type messageResponse struct {
	ID        int64  `json:"id"`
	Role      string `json:"role"`
	Content   string `json:"content"`
	IsPartial bool   `json:"isPartial"`
	CreatedAt string `json:"createdAt"`
}

func messageResponses(messages []store.Message) []messageResponse {
	responses := make([]messageResponse, 0, len(messages))
	for _, msg := range messages {
		responses = append(responses, messageResponse{
			ID:        msg.ID,
			Role:      msg.Role,
			Content:   msg.Content,
			IsPartial: msg.IsPartial,
			CreatedAt: msg.CreatedAt,
		})
	}
	return responses
}
