package models

import (
	"context"
	"encoding/json"
	"errors"
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

type failingTransport struct{}

func (failingTransport) RoundTrip(*http.Request) (*http.Response, error) {
	return nil, errors.New("network unavailable")
}

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

	cacheBytes, err := os.ReadFile(filepath.Join(dataDir, "models-cache.json"))
	if err != nil {
		t.Fatalf("cache file was not written: %v", err)
	}
	var cached cacheData
	if err := json.Unmarshal(cacheBytes, &cached); err != nil {
		t.Fatalf("cache file is not valid JSON: %v", err)
	}
	if cached.FetchedAt.IsZero() {
		t.Fatal("cache fetchedAt was not written")
	}
	cachedModel := cached.Models["gpt-test"]
	if cachedModel.ContextWindow != 128000 || cachedModel.OutputLimit != 4096 || !cachedModel.SupportsVision {
		t.Fatalf("cached model = %+v, want fetched model fields", cachedModel)
	}
}

func TestCatalogUsesStaleCacheWhenNetworkFails(t *testing.T) {
	dataDir := t.TempDir()
	cachePath := filepath.Join(dataDir, "models-cache.json")
	staleCache := `{
  "fetchedAt": "2000-01-01T00:00:00Z",
  "models": {
    "gpt-test": {
      "contextWindow": 128000,
      "outputLimit": 4096,
      "supportsVision": true
    }
  }
}`
	if err := os.WriteFile(cachePath, []byte(staleCache), 0o644); err != nil {
		t.Fatal(err)
	}

	catalog := NewCatalogWithClient(dataDir, "https://models.test/api.json", &http.Client{Transport: failingTransport{}})

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

func TestCatalogMergesDuplicateModelIDsDeterministically(t *testing.T) {
	response := `{
  "small": {
    "models": {
      "shared-model": {
        "limit": {"context": 8192, "output": 200000},
        "modalities": {"input": ["text"]}
      }
    }
  },
  "large": {
    "models": {
      "shared-model": {
        "limit": {"context": 128000, "output": 4096},
        "modalities": {"input": ["text", "image"]}
      }
    }
  },
  "same-context": {
    "models": {
      "tie-model": {
        "limit": {"context": 32000, "output": 2048},
        "modalities": {"input": ["text"]}
      }
    }
  },
  "larger-output": {
    "models": {
      "tie-model": {
        "limit": {"context": 32000, "output": 8192},
        "modalities": {"input": ["text", "image"]}
      }
    }
  }
}`
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(response))
	}))
	t.Cleanup(server.Close)

	catalog := NewCatalogWithClient(t.TempDir(), server.URL, server.Client())

	if err := catalog.Refresh(context.Background()); err != nil {
		t.Fatal(err)
	}

	if got := catalog.ContextWindow("shared-model"); got != 128000 {
		t.Fatalf("ContextWindow(shared-model) = %d, want max context 128000", got)
	}
	if got := catalog.models["shared-model"].OutputLimit; got != 4096 {
		t.Fatalf("OutputLimit(shared-model) = %d, want selected model output 4096", got)
	}
	if !catalog.SupportsVision("shared-model") {
		t.Fatal("SupportsVision(shared-model) = false, want OR result true")
	}
	if got := catalog.ContextWindow("tie-model"); got != 32000 {
		t.Fatalf("ContextWindow(tie-model) = %d, want 32000", got)
	}
	if got := catalog.models["tie-model"].OutputLimit; got != 8192 {
		t.Fatalf("OutputLimit(tie-model) = %d, want max output 8192", got)
	}
	if !catalog.SupportsVision("tie-model") {
		t.Fatal("SupportsVision(tie-model) = false, want OR result true")
	}
}
