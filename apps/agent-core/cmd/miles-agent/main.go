package main

import (
	"context"
	"flag"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"milesedgeworth/agent-core/internal/api"
	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/config"
	"milesedgeworth/agent-core/internal/chat/openai"
)

func main() {
	addr := flag.String("addr", api.DefaultListenAddr, "listen address")
	flag.Parse()

	cfg := config.FromEnv()
	debugChat := os.Getenv("MILES_DEBUG_CHAT") != ""
	openai.SetDebugLogging(debugChat)
	if debugChat {
		log.Printf("debug chat diagnostics enabled")
	}

	var provider chat.Provider
	label := "mock-fallback"
	if cfg.Enabled() {
		provider = openai.NewProvider(cfg.BaseURL, cfg.APIKey, cfg.Model, cfg.Temperature, cfg.MaxTokens)
		label = "openai-compatible"
		log.Printf("provider: openai-compatible model=%s", cfg.Model)
	} else {
		provider = chat.NewMockProvider(35 * time.Millisecond)
		log.Printf("provider: mock-fallback (set MILES_PROVIDER_BASE_URL, MILES_PROVIDER_API_KEY, MILES_PROVIDER_MODEL to use a real provider)")
	}

	server := &http.Server{
		Addr:    *addr,
		Handler: api.NewServer(provider, label).Routes(),
	}

	errs := make(chan error, 1)
	go func() {
		log.Printf("miles-agent listening on %s", *addr)
		errs <- server.ListenAndServe()
	}()

	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt, syscall.SIGTERM)

	select {
	case sig := <-signals:
		log.Printf("received %s, shutting down", sig)
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		if err := server.Shutdown(ctx); err != nil {
			log.Fatalf("shutdown failed: %v", err)
		}
	case err := <-errs:
		if err != nil && err != http.ErrServerClosed {
			log.Fatalf("server failed: %v", err)
		}
	}
}
