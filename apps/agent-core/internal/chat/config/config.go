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
	Temperature float64
	MaxTokens   int
}

const (
	defaultTemperature = 0.7
	defaultMaxTokens   = 2048
)

func FromEnv() ProviderConfig {
	return ProviderConfig{
		BaseURL:     strings.TrimSpace(os.Getenv("MILES_PROVIDER_BASE_URL")),
		APIKey:      strings.TrimSpace(os.Getenv("MILES_PROVIDER_API_KEY")),
		Model:       strings.TrimSpace(os.Getenv("MILES_PROVIDER_MODEL")),
		Temperature: parseFloat(os.Getenv("MILES_PROVIDER_TEMPERATURE"), defaultTemperature),
		MaxTokens:   parseInt(os.Getenv("MILES_PROVIDER_MAX_TOKENS"), defaultMaxTokens),
	}
}

func (c ProviderConfig) Enabled() bool {
	return c.BaseURL != "" && c.APIKey != "" && c.Model != ""
}

func parseFloat(s string, def float64) float64 {
	if strings.TrimSpace(s) == "" {
		return def
	}
	v, err := strconv.ParseFloat(strings.TrimSpace(s), 64)
	if err != nil {
		return def
	}
	return v
}

func parseInt(s string, def int) int {
	if strings.TrimSpace(s) == "" {
		return def
	}
	v, err := strconv.Atoi(strings.TrimSpace(s))
	if err != nil {
		return def
	}
	return v
}
