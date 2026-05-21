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
	if cfg.Temperature != 0.3 {
		t.Fatalf("Temperature = %v", cfg.Temperature)
	}
	if cfg.MaxTokens != 1024 {
		t.Fatalf("MaxTokens = %v", cfg.MaxTokens)
	}
	if !cfg.Enabled() {
		t.Fatal("Enabled should be true when all required env set")
	}
}

func TestFromEnvDefaults(t *testing.T) {
	t.Setenv("MILES_PROVIDER_BASE_URL", "")
	t.Setenv("MILES_PROVIDER_API_KEY", "")
	t.Setenv("MILES_PROVIDER_MODEL", "")
	t.Setenv("MILES_PROVIDER_TEMPERATURE", "")
	t.Setenv("MILES_PROVIDER_MAX_TOKENS", "")

	cfg := config.FromEnv()

	if cfg.Temperature != 0.7 {
		t.Fatalf("default Temperature should be 0.7, got %v", cfg.Temperature)
	}
	if cfg.MaxTokens != 2048 {
		t.Fatalf("default MaxTokens should be 2048, got %v", cfg.MaxTokens)
	}
	if cfg.Enabled() {
		t.Fatal("Enabled should be false when required env missing")
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
