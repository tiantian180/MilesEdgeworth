package models

import (
	"context"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"
)

const testCatalogJSON = `{
  "openai": {
    "models": {
      "gpt-test": {
        "limit": {"context": 128000, "output": 4096},
        "modalities": {"input": ["text", "image"]}
      }
    }
  }
}`

func TestCatalogRefreshLoadsModelCapabilitiesAndWritesCache(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(testCatalogJSON))
	}))
	t.Cleanup(server.Close)

	dataDir := t.TempDir()
	catalog := NewCatalogWithClient(dataDir, server.URL, server.Client())

	if err := catalog.Refresh(context.Background()); err != nil {
		t.Fatal(err)
	}

	if got := catalog.ContextWindow("gpt-test"); got != 128000 {
		t.Fatalf("ContextWindow(gpt-test) = %d, want 128000", got)
	}
	if !catalog.SupportsVision("gpt-test") {
		t.Fatal("SupportsVision(gpt-test) = false, want true")
	}
	if got := catalog.ContextWindow("unknown-model"); got != 8192 {
		t.Fatalf("ContextWindow(unknown-model) = %d, want 8192", got)
	}
	if _, err := os.Stat(filepath.Join(dataDir, "models-cache.json")); err != nil {
		t.Fatalf("cache file was not written: %v", err)
	}
}

func TestCatalogUsesStaleCacheWhenNetworkFails(t *testing.T) {
	dataDir := t.TempDir()
	cachePath := filepath.Join(dataDir, "models-cache.json")
	staleCache := `{
  "fetchedAt": "2000-01-01T00:00:00Z",
  "models": {
    "gpt-test": {
      "ContextWindow": 128000,
      "OutputLimit": 4096,
      "SupportsVision": true
    }
  }
}`
	if err := os.WriteFile(cachePath, []byte(staleCache), 0o644); err != nil {
		t.Fatal(err)
	}

	catalog := NewCatalogWithClient(dataDir, "http://127.0.0.1:1/api.json", &http.Client{})

	if err := catalog.Refresh(context.Background()); err != nil {
		t.Fatal(err)
	}

	if got := catalog.ContextWindow("gpt-test"); got != 128000 {
		t.Fatalf("ContextWindow(gpt-test) = %d, want stale cache value 128000", got)
	}
	if !catalog.SupportsVision("gpt-test") {
		t.Fatal("SupportsVision(gpt-test) = false, want stale cache value true")
	}
}
