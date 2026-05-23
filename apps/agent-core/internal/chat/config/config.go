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
	Langfuse    LangfuseConfig
}

type LangfuseConfig struct {
	EnabledFlag    bool
	Host           string
	PublicKey      string
	SecretKey      string
	CaptureContent bool
}

func FromEnv() ProviderConfig {
	return ProviderConfig{
		BaseURL:     strings.TrimSpace(os.Getenv("MILES_PROVIDER_BASE_URL")),
		APIKey:      strings.TrimSpace(os.Getenv("MILES_PROVIDER_API_KEY")),
		Model:       strings.TrimSpace(os.Getenv("MILES_PROVIDER_MODEL")),
		Temperature: parseFloat(os.Getenv("MILES_PROVIDER_TEMPERATURE")),
		MaxTokens:   parseInt(os.Getenv("MILES_PROVIDER_MAX_TOKENS")),
		Langfuse: LangfuseConfig{
			EnabledFlag:    parseBool(os.Getenv("MILES_LANGFUSE_ENABLED"), false),
			Host:           strings.TrimSpace(os.Getenv("LANGFUSE_HOST")),
			PublicKey:      strings.TrimSpace(os.Getenv("LANGFUSE_PUBLIC_KEY")),
			SecretKey:      strings.TrimSpace(os.Getenv("LANGFUSE_SECRET_KEY")),
			CaptureContent: parseBool(os.Getenv("MILES_LANGFUSE_CAPTURE_CONTENT"), true),
		},
	}
}

func (c ProviderConfig) Enabled() bool {
	return c.BaseURL != "" && c.APIKey != "" && c.Model != ""
}

func (c LangfuseConfig) Enabled() bool {
	return c.EnabledFlag && c.Host != "" && c.PublicKey != "" && c.SecretKey != ""
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

func parseBool(s string, defaultValue bool) bool {
	switch strings.ToLower(strings.TrimSpace(s)) {
	case "":
		return defaultValue
	case "1", "true", "yes", "on":
		return true
	case "0", "false", "no", "off":
		return false
	default:
		return defaultValue
	}
}
