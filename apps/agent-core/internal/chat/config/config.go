// Package config exposes provider configuration sourced from environment variables.
package config

import (
	"os"
	"strconv"
	"strings"
)

type ProviderConfig struct {
	BaseURL     string
	APIKey      string
	Model       string
	Temperature *float64
	MaxTokens   *int
}

func FromEnv() ProviderConfig {
	return ProviderConfig{
		BaseURL:     strings.TrimSpace(os.Getenv("MILES_PROVIDER_BASE_URL")),
		APIKey:      strings.TrimSpace(os.Getenv("MILES_PROVIDER_API_KEY")),
		Model:       strings.TrimSpace(os.Getenv("MILES_PROVIDER_MODEL")),
		Temperature: parseFloat(os.Getenv("MILES_PROVIDER_TEMPERATURE")),
		MaxTokens:   parseInt(os.Getenv("MILES_PROVIDER_MAX_TOKENS")),
	}
}

func (c ProviderConfig) Enabled() bool {
	return c.BaseURL != "" && c.APIKey != "" && c.Model != ""
}

func parseFloat(s string) *float64 {
	if strings.TrimSpace(s) == "" {
		return nil
	}
	v, err := strconv.ParseFloat(strings.TrimSpace(s), 64)
	if err != nil {
		return nil
	}
	return &v
}

func parseInt(s string) *int {
	if strings.TrimSpace(s) == "" {
		return nil
	}
	v, err := strconv.Atoi(strings.TrimSpace(s))
	if err != nil || v < 1 {
		return nil
	}
	return &v
}
