package models

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"time"
)

const DefaultContextWindow = 8192
const cacheFileName = "models-cache.json"
const defaultEndpoint = "https://models.dev/api.json"

const cacheTTL = 7 * 24 * time.Hour

type Catalog struct {
	dataDir  string
	endpoint string
	client   *http.Client
	models   map[string]ModelInfo
}

type ModelInfo struct {
	ContextWindow  int  `json:"contextWindow"`
	OutputLimit    int  `json:"outputLimit"`
	SupportsVision bool `json:"supportsVision"`
}

type cacheData struct {
	FetchedAt time.Time            `json:"fetchedAt"`
	Models    map[string]ModelInfo `json:"models"`
}

func NewCatalog(dataDir string) *Catalog {
	return NewCatalogWithClient(dataDir, defaultEndpoint, &http.Client{Timeout: 10 * time.Second})
}

func NewCatalogWithClient(dataDir, endpoint string, client *http.Client) *Catalog {
	if client == nil {
		client = &http.Client{Timeout: 10 * time.Second}
	}
	return &Catalog{
		dataDir:  dataDir,
		endpoint: endpoint,
		client:   client,
		models:   make(map[string]ModelInfo),
	}
}

func (c *Catalog) Refresh(ctx context.Context) error {
	cached, hasCache := c.readCache()
	if hasCache && time.Since(cached.FetchedAt) < cacheTTL {
		c.models = cached.Models
		return nil
	}

	models, err := c.fetchModels(ctx)
	if err != nil {
		if hasCache {
			c.models = cached.Models
		}
		return nil
	}

	c.models = models
	return c.writeCache(cacheData{
		FetchedAt: time.Now(),
		Models:    models,
	})
}

func (c *Catalog) ContextWindow(modelID string) int {
	info, ok := c.models[modelID]
	if !ok || info.ContextWindow <= 0 {
		return DefaultContextWindow
	}
	return info.ContextWindow
}

func (c *Catalog) SupportsVision(modelID string) bool {
	return c.models[modelID].SupportsVision
}

func (c *Catalog) readCache() (cacheData, bool) {
	data, err := os.ReadFile(filepath.Join(c.dataDir, cacheFileName))
	if err != nil {
		return cacheData{}, false
	}
	var cached cacheData
	if err := json.Unmarshal(data, &cached); err != nil || cached.Models == nil {
		return cacheData{}, false
	}
	return cached, true
}

func (c *Catalog) writeCache(data cacheData) error {
	if err := os.MkdirAll(c.dataDir, 0o755); err != nil {
		return err
	}
	encoded, err := json.MarshalIndent(data, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(filepath.Join(c.dataDir, cacheFileName), encoded, 0o644)
}

func (c *Catalog) fetchModels(ctx context.Context) (map[string]ModelInfo, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, c.endpoint, nil)
	if err != nil {
		return nil, err
	}
	resp, err := c.client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("models catalog status %d", resp.StatusCode)
	}

	var providers map[string]struct {
		Models map[string]struct {
			Limit struct {
				Context int `json:"context"`
				Output  int `json:"output"`
			} `json:"limit"`
			Modalities struct {
				Input []string `json:"input"`
			} `json:"modalities"`
		} `json:"models"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&providers); err != nil {
		return nil, err
	}

	models := make(map[string]ModelInfo)
	for _, provider := range providers {
		for id, raw := range provider.Models {
			models[id] = ModelInfo{
				ContextWindow:  raw.Limit.Context,
				OutputLimit:    raw.Limit.Output,
				SupportsVision: contains(raw.Modalities.Input, "image"),
			}
		}
	}
	return models, nil
}

func contains(values []string, target string) bool {
	for _, value := range values {
		if value == target {
			return true
		}
	}
	return false
}
