package api

import (
	"encoding/json"
	"net/http"
	"os"
	"strings"

	"milesedgeworth/agent-core/internal/chat"
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
	provider      chat.Provider
	providerLabel string
}

func NewServer(provider chat.Provider, providerLabel string) *Server {
	if providerLabel == "" {
		providerLabel = "unknown"
	}
	return &Server{provider: provider, providerLabel: providerLabel}
}

func (s *Server) Routes() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/health", s.handleHealth)
	mux.HandleFunc("/v1/chat/messages", s.handleChatMessages)
	return mux
}

func (s *Server) handleHealth(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	writeJSON(w, http.StatusOK, map[string]any{
		"ok":       true,
		"pid":      os.Getpid(),
		"provider": s.providerLabel,
		"service":  "miles-agent",
	})
}

func (s *Server) handleChatMessages(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
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

	events, err := s.provider.StreamReply(r.Context(), req)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]any{"error": err.Error()})
		return
	}

	w.Header().Set("Content-Type", "text/event-stream; charset=utf-8")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.WriteHeader(http.StatusOK)

	flusher, _ := w.(http.Flusher)
	for event := range events {
		if err := writeSSE(w, event); err != nil {
			return
		}
		if flusher != nil {
			flusher.Flush()
		}
	}
}

func writeJSON(w http.ResponseWriter, status int, body any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(body)
}

func writeSSE(w http.ResponseWriter, event chat.StreamEvent) error {
	body, err := json.Marshal(event)
	if err != nil {
		return err
	}
	_, err = w.Write(append(append([]byte("data: "), body...), '\n', '\n'))
	return err
}
