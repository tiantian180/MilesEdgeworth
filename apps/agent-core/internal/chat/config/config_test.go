package config_test

import (
	"testing"

	"milesedgeworth/agent-core/internal/chat/config"
)

func TestFromEnv(t *testing.T) {
	t.Setenv("MILES_PROVIDER_BASE_URL", "https://api.example.com")
	t.Setenv("MILES_PROVIDER_API_KEY", "sk-test")
	t.Setenv("MILES_PROVIDER_MODEL", "gpt-test")
	t.Setenv("MILES_PROVIDER_TEMPERATURE", "0.3")
	t.Setenv("MILES_PROVIDER_MAX_TOKENS", "1024")
	t.Setenv("MILES_LANGFUSE_ENABLED", "1")
	t.Setenv("LANGFUSE_HOST", "https://cloud.langfuse.com")
	t.Setenv("LANGFUSE_PUBLIC_KEY", "pk-lf-test")
	t.Setenv("LANGFUSE_SECRET_KEY", "sk-lf-test")
	t.Setenv("MILES_LANGFUSE_CAPTURE_CONTENT", "0")

	cfg := config.FromEnv()

	if cfg.BaseURL != "https://api.example.com" {
		t.Fatalf("BaseURL = %q", cfg.BaseURL)
	}
	if cfg.APIKey != "sk-test" {
		t.Fatalf("APIKey = %q", cfg.APIKey)
	}
	if cfg.Model != "gpt-test" {
		t.Fatalf("Model = %q", cfg.Model)
	}
	if cfg.Temperature == nil || *cfg.Temperature != 0.3 {
		t.Fatalf("Temperature = %v", cfg.Temperature)
	}
	if cfg.MaxTokens == nil || *cfg.MaxTokens != 1024 {
		t.Fatalf("MaxTokens = %v", cfg.MaxTokens)
	}
	if !cfg.Enabled() {
		t.Fatal("Enabled should be true when all required env set")
	}
	if !cfg.Langfuse.Enabled() {
		t.Fatal("Langfuse should be enabled when all required env set")
	}
	if cfg.Langfuse.Host != "https://cloud.langfuse.com" {
		t.Fatalf("Langfuse.Host = %q", cfg.Langfuse.Host)
	}
	if cfg.Langfuse.PublicKey != "pk-lf-test" {
		t.Fatalf("Langfuse.PublicKey = %q", cfg.Langfuse.PublicKey)
	}
	if cfg.Langfuse.SecretKey != "sk-lf-test" {
		t.Fatalf("Langfuse.SecretKey = %q", cfg.Langfuse.SecretKey)
	}
	if cfg.Langfuse.CaptureContent {
		t.Fatal("Langfuse.CaptureContent should be false when env is 0")
	}
}

func TestFromEnvDefaults(t *testing.T) {
	t.Setenv("MILES_PROVIDER_BASE_URL", "")
	t.Setenv("MILES_PROVIDER_API_KEY", "")
	t.Setenv("MILES_PROVIDER_MODEL", "")
	t.Setenv("MILES_PROVIDER_TEMPERATURE", "")
	t.Setenv("MILES_PROVIDER_MAX_TOKENS", "")
	t.Setenv("MILES_LANGFUSE_ENABLED", "")
	t.Setenv("LANGFUSE_HOST", "")
	t.Setenv("LANGFUSE_PUBLIC_KEY", "")
	t.Setenv("LANGFUSE_SECRET_KEY", "")
	t.Setenv("MILES_LANGFUSE_CAPTURE_CONTENT", "")

	cfg := config.FromEnv()

	if cfg.Temperature != nil {
		t.Fatalf("default Temperature should be nil, got %v", cfg.Temperature)
	}
	if cfg.MaxTokens != nil {
		t.Fatalf("default MaxTokens should be nil, got %v", cfg.MaxTokens)
	}
	if cfg.Enabled() {
		t.Fatal("Enabled should be false when required env missing")
	}
	if cfg.Langfuse.Enabled() {
		t.Fatal("Langfuse should be disabled by default")
	}
	if !cfg.Langfuse.CaptureContent {
		t.Fatal("Langfuse should capture content by default when enabled later")
	}
}

func TestLangfuseEnabledRequiresToggleAndCredentials(t *testing.T) {
	cases := []struct {
		name                  string
		enabled, host, pk, sk string
		want                  bool
	}{
		{"complete", "1", "https://cloud.langfuse.com", "pk", "sk", true},
		{"disabled", "0", "https://cloud.langfuse.com", "pk", "sk", false},
		{"missing host", "1", "", "pk", "sk", false},
		{"missing public key", "1", "https://cloud.langfuse.com", "", "sk", false},
		{"missing secret key", "1", "https://cloud.langfuse.com", "pk", "", false},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			t.Setenv("MILES_LANGFUSE_ENABLED", tc.enabled)
			t.Setenv("LANGFUSE_HOST", tc.host)
			t.Setenv("LANGFUSE_PUBLIC_KEY", tc.pk)
			t.Setenv("LANGFUSE_SECRET_KEY", tc.sk)
			if got := config.FromEnv().Langfuse.Enabled(); got != tc.want {
				t.Fatalf("Langfuse.Enabled() = %v, want %v", got, tc.want)
			}
		})
	}
}

func TestEnabledRequiresAllThree(t *testing.T) {
	cases := []struct {
		name              string
		baseURL, key, mdl string
		want              bool
	}{
		{"all set", "https://x", "k", "m", true},
		{"missing baseURL", "", "k", "m", false},
		{"missing key", "https://x", "", "m", false},
		{"missing model", "https://x", "k", "", false},
		{"whitespace only", "  ", "k", "m", false},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			t.Setenv("MILES_PROVIDER_BASE_URL", tc.baseURL)
			t.Setenv("MILES_PROVIDER_API_KEY", tc.key)
			t.Setenv("MILES_PROVIDER_MODEL", tc.mdl)
			if got := config.FromEnv().Enabled(); got != tc.want {
				t.Fatalf("Enabled() = %v, want %v", got, tc.want)
			}
		})
	}
}
