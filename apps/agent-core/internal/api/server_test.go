package api_test

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"milesedgeworth/agent-core/internal/api"
	"milesedgeworth/agent-core/internal/chat"
)

func TestHealth(t *testing.T) {
	server := httptest.NewServer(api.NewServer(chat.NewMockProvider(0)).Routes())
	defer server.Close()

	resp, err := http.Get(server.URL + "/health")
	if err != nil {
		t.Fatalf("GET /health failed: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}

	var body map[string]any
	if err := json.NewDecoder(resp.Body).Decode(&body); err != nil {
		t.Fatalf("decode health: %v", err)
	}
	if body["ok"] != true {
		t.Fatalf("health ok = %v, want true", body["ok"])
	}
	if body["provider"] != "mock" {
		t.Fatalf("provider = %v, want mock", body["provider"])
	}
}

func TestMockChatStream(t *testing.T) {
	server := httptest.NewServer(api.NewServer(chat.NewMockProvider(0)).Routes())
	defer server.Close()

	payload := []byte(`{"conversationId":"default","message":"hello miles"}`)
	resp, err := http.Post(server.URL+"/v1/chat/messages", "application/json", bytes.NewReader(payload))
	if err != nil {
		t.Fatalf("POST /v1/chat/messages failed: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status = %d, want 200", resp.StatusCode)
	}
	if got := resp.Header.Get("Content-Type"); !strings.Contains(got, "text/event-stream") {
		t.Fatalf("content-type = %q, want text/event-stream", got)
	}

	bodyBytes, err := io.ReadAll(resp.Body)
	if err != nil {
		t.Fatalf("read stream: %v", err)
	}
	body := string(bodyBytes)
	for _, token := range []string{
		`"type":"RUN_STARTED"`,
		`"name":"miles.pet.expression.requested"`,
		`"state":"thinking"`,
		`"expression":"neutral"`,
		`"type":"TEXT_MESSAGE_START"`,
		`"expression":"objection"`,
		`"type":"TEXT_MESSAGE_CONTENT"`,
		`"type":"TEXT_MESSAGE_END"`,
		`"type":"RUN_FINISHED"`,
	} {
		if !strings.Contains(body, token) {
			t.Fatalf("stream missing %s in:\n%s", token, body)
		}
	}
}

func TestChatStreamRejectsEmptyMessage(t *testing.T) {
	server := httptest.NewServer(api.NewServer(chat.NewMockProvider(time.Millisecond)).Routes())
	defer server.Close()

	resp, err := http.Post(server.URL+"/v1/chat/messages", "application/json", strings.NewReader(`{"message":"   "}`))
	if err != nil {
		t.Fatalf("POST /v1/chat/messages failed: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusBadRequest {
		t.Fatalf("status = %d, want 400", resp.StatusCode)
	}
}
