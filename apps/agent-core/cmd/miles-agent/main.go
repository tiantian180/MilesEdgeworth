package main

import (
	"context"
	"flag"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"milesedgeworth/agent-core/internal/api"
	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/config"
	"milesedgeworth/agent-core/internal/chat/openai"
	chatservice "milesedgeworth/agent-core/internal/chat/service"
	"milesedgeworth/agent-core/internal/mileslog"
	"milesedgeworth/agent-core/internal/models"
	"milesedgeworth/agent-core/internal/store"
)

var logger = mileslog.New("MILES.SIDECAR")

func main() {
	addr := flag.String("addr", api.DefaultListenAddr, "listen address")
	flag.Parse()

	cfg := config.FromEnv()

	var provider chat.Provider
	label := "mock-fallback"
	if cfg.Enabled() {
		provider = openai.NewProvider(cfg.BaseURL, cfg.APIKey, cfg.Model, cfg.Temperature, cfg.MaxTokens)
		label = "openai-compatible"
		logger.Info("provider selected", "provider", "openai-compatible", "model", cfg.Model)
	} else {
		provider = chat.NewMockProvider(35 * time.Millisecond)
		logger.Info("provider selected", "provider", "mock-fallback")
	}

	dataDir := os.Getenv("MILES_DATA_DIR")
	if strings.TrimSpace(dataDir) == "" {
		dataDir = filepath.Join(os.TempDir(), "MilesEdgeworth")
	}
	st, err := store.Open(dataDir)
	if err != nil {
		logger.Error("store open failed", "error", err)
		os.Exit(1)
	}
	defer st.Close()

	catalog := models.NewCatalog(dataDir)
	if err := catalog.Refresh(context.Background()); err != nil {
		logger.Warn("models catalog refresh failed", "error", err)
	}
	chatService := chatservice.New(st, provider, catalog, cfg.Model)

	server := &http.Server{
		Addr:    *addr,
		Handler: api.NewServer(st, chatService, label).Routes(),
	}

	errs := make(chan error, 1)
	go func() {
		logger.Info("server listening", "addr", *addr)
		errs <- server.ListenAndServe()
	}()

	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt, syscall.SIGTERM)

	select {
	case sig := <-signals:
		logger.Info("shutdown requested", "signal", sig.String())
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		if err := server.Shutdown(ctx); err != nil {
			logger.Error("shutdown failed", "error", err)
			os.Exit(1)
		}
	case err := <-errs:
		if err != nil && err != http.ErrServerClosed {
			logger.Error("server failed", "error", err)
			os.Exit(1)
		}
	}
}
